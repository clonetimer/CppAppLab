"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const net = require("node:net");
const { spawn } = require("node:child_process");
const { chromium } = require("playwright");

const ROOT = path.resolve(process.env.CPPAPPLAB_TEST_ROOT || path.join(__dirname, ".."));
const BINARY = process.env.CPPAPPLAB_TEST_BINARY;
const ARTIFACTS = process.env.CPPAPPLAB_BROWSER_ARTIFACT_DIR || path.join(os.tmpdir(), "cppapplab-learning-space-artifacts");
const BASELINE = process.argv.includes("--baseline");
const EDITOR_CDN = /https?:\/\/(cdn\.jsdelivr\.net|gcore\.jsdelivr\.net|cdnjs\.cloudflare\.com)\//;
const HELLO = '#include <iostream>\nint main() { std::cout << "Hello, CppAppLab!\\n"; }\n';
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function contrastRatio(foreground, background) {
  const luminance = (color) => {
    const values = color.match(/[\d.]+/g)?.map(Number) || [];
    assert.ok(values.length === 3 || (values.length === 4 && values[3] === 1),
      "Button contrast evidence requires opaque computed RGB colors: " + color);
    const linear = values.slice(0, 3).map((value) => {
      const channel = value / 255;
      return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4;
    });
    return linear[0] * 0.2126 + linear[1] * 0.7152 + linear[2] * 0.0722;
  };
  const first = luminance(foreground), second = luminance(background);
  return (Math.max(first, second) + 0.05) / (Math.min(first, second) + 0.05);
}

async function waitWithin(promise, timeout, message) {
  let timer;
  try {
    return await Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), timeout);
    })]);
  } finally { clearTimeout(timer); }
}

async function freePort() {
  const server = net.createServer();
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const port = server.address().port;
  await new Promise((resolve) => server.close(resolve));
  return port;
}

async function fixture(browser) {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cppapplab-learning-space-"));
  const root = path.join(temporary, "repo");
  const home = path.join(temporary, "home");
  const workspace = path.join(home, "cppapplab-work");
  const state = path.join(temporary, "state");
  fs.mkdirSync(root);
  fs.mkdirSync(workspace, { recursive: true });
  for (const directory of ["course", "labs", "web"]) {
    fs.cpSync(path.join(ROOT, directory), path.join(root, directory), { recursive: true });
  }
  const baseUrl = "http://127.0.0.1:" + await freePort();
  const child = spawn("python3", [path.join(root, "web", "server", "cppapplab_web.py"),
    "--root", root, "--cppapplab", BINARY, "--host", "127.0.0.1", "--port", baseUrl.split(":").pop(),
    "--workspace-root", workspace], {
    env: { ...process.env, HOME: home, CPPAPPLAB_STATE_DIR: state, PYTHONDONTWRITEBYTECODE: "1" },
    stdio: ["ignore", "ignore", "pipe"]
  });
  let errors = "";
  child.stderr.on("data", (data) => { errors += data; });
  const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
  await context.route(EDITOR_CDN, (route) => route.abort());
  const page = await context.newPage();
  page.setDefaultTimeout(10000);
  const pageErrors = [];
  page.on("pageerror", (error) => pageErrors.push(error.message));
  async function close() {
    try {
      await waitWithin(context.close(), 10000, "Timed out closing the browser context");
    } finally {
      try {
        if (child.exitCode === null && child.signalCode === null) {
          const stopped = new Promise((resolve) => child.once("exit", resolve));
          child.kill("SIGTERM");
          try { await waitWithin(stopped, 5000, "Timed out stopping the learning server"); }
          catch {
            child.kill("SIGKILL");
            await waitWithin(stopped, 5000, "Learning server did not stop after SIGKILL");
          }
        }
      } finally { fs.rmSync(temporary, { recursive: true, force: true }); }
    }
  }
  try {
    let ready = false;
    for (let attempt = 0; attempt < 100; attempt += 1) {
      if (child.exitCode !== null) throw new Error(errors);
      try {
        if ((await fetch(baseUrl + "/api/v1/health")).ok) { ready = true; break; }
      } catch { /* The real loopback server is still starting. */ }
      await sleep(50);
    }
    assert.ok(ready, "real learning server must start: " + errors);
    await page.goto(baseUrl);
    await page.waitForFunction(() => document.querySelectorAll("#course-list .task-link").length === 23);
  } catch (error) {
    await close();
    throw error;
  }
  return { page, context, root, workspace, baseUrl, pageErrors, close };
}

async function selectTask(page, taskId) {
  await page.locator('#course-list [data-task-id="' + taskId + '"]').click();
  await page.waitForFunction((id) => document.querySelector("#task-breadcrumb").textContent === id &&
    !document.querySelector('#course-list [data-task-id="' + id + '"]').disabled, taskId);
}

async function clickWorkbenchAction(page, selector) {
  const group = page.locator("#editor-actions-group");
  if (!(await group.evaluate((element) => element.open))) await group.locator("summary").click();
  await page.locator(selector).click();
}

async function position(page, file) {
  await page.waitForFunction((file) => document.querySelector("#task-breadcrumb").textContent === "w1.calculator" &&
    document.querySelector("#active-file").textContent === file &&
    !document.querySelector("#source-editor").disabled &&
    !document.querySelector('#course-list [data-task-id="w1.calculator"]').disabled, file);
}

async function startCalculator(page) {
  await selectTask(page, "w1.hello-world");
  await page.locator("#start-button").click();
  await page.waitForFunction(() => !document.querySelector("#source-editor").disabled);
  await page.locator("#source-editor").fill(HELLO);
  await clickWorkbenchAction(page, "#save-button");
  await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
  await clickWorkbenchAction(page, "#check-button");
  await page.waitForFunction(() => {
    const calculator = document.querySelector('#course-list [data-task-id="w1.calculator"]');
    return document.querySelector("#check-result").textContent === "PASS" &&
      document.querySelector("#task-state").dataset.state === "PASSED" &&
      calculator?.dataset.state === "AVAILABLE" && !calculator.disabled;
  }, null, { timeout: 60000 });
  if (!BASELINE) await checkHistoryViewport(page);
  await selectTask(page, "w1.calculator");
  await page.locator("#start-button").click();
  await position(page, "include/calculator.hpp");
  await page.locator('#file-list button[title="src/calculator.cpp"]').click();
  await position(page, "src/calculator.cpp");
  const actions = page.locator("#editor-actions-group");
  if (await actions.evaluate((element) => element.open)) await actions.locator("summary").click();
}

const INVALID_SOURCE = '#include "calculator.hpp"\nnamespace cppapplab::calculator {\nResult calculate(Operation, double, double) noexcept { return CPPAPPLAB_SPACE_ERROR; }\n}\n';
const report = { baseline: BASELINE, root: ROOT, scenarios: [], theme_checks: [] };

async function checkHistoryViewport(page) {
  await page.setViewportSize({ width: 1366, height: 768 });
  assert.equal(await page.locator("#console-dock").getAttribute("data-mode"), "normal");
  await page.locator("#history-button").click();
  await page.waitForFunction(() => !document.querySelector("#history-panel").hidden &&
    document.querySelectorAll("#history-list .history-record").length === 1);
  const record = page.locator("#history-list .history-record").last();
  await record.scrollIntoViewIfNeeded();
  const geometry = await record.evaluate((element) => {
    const box = element.getBoundingClientRect();
    const dock = document.querySelector("#console-dock").getBoundingClientRect();
    let top = Math.max(0, box.top), bottom = Math.min(innerHeight, dock.top, box.bottom);
    for (let ancestor = element.parentElement; ancestor; ancestor = ancestor.parentElement) {
      if (!/hidden|auto|scroll|clip/.test(getComputedStyle(ancestor).overflowY)) continue;
      const clip = ancestor.getBoundingClientRect();
      top = Math.max(top, clip.top); bottom = Math.min(bottom, clip.bottom);
    }
    const fingerprint = element.querySelector("code").getBoundingClientRect();
    return { height: box.height, visibleHeight: Math.max(0, bottom - top),
      fingerprintVisible: fingerprint.top >= top && fingerprint.bottom <= bottom + 1,
      text: element.textContent, dockTop: dock.top, top: box.top, bottom: box.bottom };
  });
  assert.ok(geometry.height >= 80 && geometry.visibleHeight >= geometry.height - 1 && geometry.fingerprintVisible,
    "The real acceptance record, including its last source fingerprint line, must be fully reachable above the dock on a short desktop: " + JSON.stringify(geometry));
  assert.match(geometry.text, /通过.*测试结果：.*工作区：.*源码指纹：/s);
  report.scenarios.push({ scenario: "real-history-short-desktop", viewport: { width: 1366, height: 768 }, ...geometry });
  await page.screenshot({ path: path.join(ARTIFACTS, "learning-space-after-history-1366x768.png"), fullPage: false });
  await page.locator("#history-close").click();
  await page.setViewportSize({ width: 1440, height: 1000 });
  console.log("PASS: learning space / actual acceptance history remains fully readable on a short desktop");
}

async function resetReadingPosition(page) {
  await page.evaluate(() => {
    window.scrollTo(0, 0);
    for (const selector of [".main-content", "#lesson-content", "#source-editor"]) {
      const element = document.querySelector(selector);
      if (element) { element.scrollTop = 0; element.scrollLeft = 0; }
    }
  });
}

async function measurements(page) {
  return page.evaluate(() => {
    const dock = document.querySelector("#console-dock");
    const dockBox = dock.getBoundingClientRect();
    const describe = (element) => {
      if (!element) return null;
      const rect = element.getBoundingClientRect();
      const style = getComputedStyle(element);
      let left = Math.max(0, rect.left), right = Math.min(innerWidth, rect.right);
      let top = Math.max(0, rect.top), bottom = Math.min(innerHeight, rect.bottom, dockBox.top);
      for (let ancestor = element.parentElement; ancestor; ancestor = ancestor.parentElement) {
        const clipping = getComputedStyle(ancestor);
        const box = ancestor.getBoundingClientRect();
        if (/hidden|auto|scroll|clip/.test(clipping.overflowX)) {
          left = Math.max(left, box.left); right = Math.min(right, box.right);
        }
        if (/hidden|auto|scroll|clip/.test(clipping.overflowY)) {
          top = Math.max(top, box.top); bottom = Math.min(bottom, box.bottom);
        }
      }
      const hidden = style.display === "none" || style.visibility === "hidden" || element.hidden;
      return { top: rect.top, bottom: rect.bottom, width: rect.width, height: rect.height,
        visibleHeight: hidden ? 0 : Math.max(0, bottom - top),
        visibleWidth: hidden ? 0 : Math.max(0, right - left), lineHeight: parseFloat(style.lineHeight),
        fontSize: parseFloat(style.fontSize), text: element.textContent.slice(0, 180) };
    };
    const prose = [...document.querySelectorAll("#lesson-content p")].find((element) => element.textContent.trim().length > 20);
    const status = document.querySelector("#workspace-state");
    const button = document.querySelector("#workflow-action");
    const buttonStyle = getComputedStyle(button);
    return { viewport: { width: innerWidth, height: innerHeight }, pageWidth: document.documentElement.scrollWidth,
      dock: { top: dockBox.top, bottom: dockBox.bottom, height: dockBox.height, state: dock.dataset.state,
        mode: dock.dataset.mode, collapsed: dock.classList.contains("is-collapsed") },
      lesson: describe(document.querySelector("#lesson-content")), prose: describe(prose),
      editor: describe(document.querySelector("#source-editor")),
      workspace: { tag: status.tagName, text: status.textContent, parent: status.parentElement.className },
      nextAction: { text: button.textContent, hidden: button.hidden, disabled: button.disabled,
        pending: button.dataset.pending, busy: button.getAttribute("aria-busy"), action: button.dataset.action,
        color: buttonStyle.color, background: buttonStyle.backgroundColor,
        backgroundImage: buttonStyle.backgroundImage, opacity: buttonStyle.opacity },
      result: document.querySelector("#check-result").textContent,
      saveState: document.querySelector("#save-state").textContent,
      summary: document.querySelector("#console-summary")?.textContent || "" };
  });
}

async function evidence(page, scenario, dimensions) {
  if (dimensions) await page.setViewportSize(dimensions);
  await resetReadingPosition(page);
  const metrics = await measurements(page);
  report.scenarios.push({ scenario, ...metrics });
  const stem = "learning-space-" + (BASELINE ? "before" : "after") + "-" + scenario + "-" +
    metrics.viewport.width + "x" + metrics.viewport.height;
  await page.screenshot({ path: path.join(ARTIFACTS, stem + ".png"), fullPage: false });
  return metrics;
}

function assertReadingSpace(metrics) {
  const { viewport, dock, prose, editor } = metrics;
  assert.ok(metrics.pageWidth <= viewport.width + 1, "page must not overflow horizontally: " + JSON.stringify(metrics));
  assert.ok(dock.height <= 72 && dock.top >= 0 && dock.bottom <= viewport.height + 1,
    "idle terminal retains a compact, visible status bar: " + JSON.stringify(dock));
  assert.ok(prose && prose.visibleHeight >= prose.lineHeight * 2 - 1 && prose.visibleWidth >= 180,
    "two actual tutorial prose lines must be visible at first opening, above the idle terminal: " + JSON.stringify(metrics));
  if (viewport.width > 820) {
    const minimum = viewport.height >= 900 ? 180 : 120;
    assert.ok(editor.visibleHeight >= minimum && editor.visibleWidth >= 300,
      "the source editor must expose useful code space alongside the lesson without scrolling: " + JSON.stringify(metrics));
  }
}

async function themeEvidence(page) {
  // Real DOM/browser checks for both system themes. The historical page is
  // recorded as-is; the candidate must retain readable status and prose.
  for (const colorScheme of ["light", "dark"]) {
    await page.emulateMedia({ colorScheme });
    // Capture the settled palette, not a finite hover/color transition.
    await page.waitForTimeout(200);
    await resetReadingPosition(page);
    const actual = await page.evaluate(() => {
      const read = (selector) => {
        const style = getComputedStyle(document.querySelector(selector));
        return { color: style.color, background: style.backgroundColor, image: style.backgroundImage };
      };
      return {
        body: read("body"), panel: read(".lesson-panel"), prose: read("#lesson-content"),
        editor: read("#source-editor"), next: read("#workflow-action"),
        theme: getComputedStyle(document.documentElement).colorScheme,
        visibleWidth: document.documentElement.scrollWidth,
        decorativeMark: getComputedStyle(document.querySelector(".brand-mark")).display,
      };
    });
    report.theme_checks.push({ colorScheme, ...actual });
    await evidence(page, "theme-" + colorScheme, { width: 1440, height: 1000 });
    if (!BASELINE) {
      assert.equal(actual.body.image, "none");
      assert.equal(actual.next.image, "none");
      assert.equal(actual.decorativeMark, "none");
      assert.ok(contrastRatio(actual.body.color, actual.body.background) >= 4.5);
      assert.ok(contrastRatio(actual.prose.color, actual.panel.background) >= 4.5);
      assert.ok(contrastRatio(actual.editor.color, actual.editor.background) >= 4.5);
      assert.ok(contrastRatio(actual.next.color, actual.next.background) >= 4.5);
      assertReadingSpace(await measurements(page));
    }
  }
  if (!BASELINE) assert.notEqual(report.theme_checks[0].body.background, report.theme_checks[1].body.background);
  await page.emulateMedia({ colorScheme: "light" });
}

async function knowledgeEvidence(page, baseUrl) {
  const editor = page.locator("#source-editor");
  const source = await editor.inputValue();
  const priorResult = await page.locator("#check-result").textContent();
  const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w1.calculator/history")).json();
  await page.locator("#knowledge-open").click();
  await page.locator("#knowledge-search").fill("unique_ptr");
  await page.locator('#knowledge-results [data-topic="ownership"]').click();
  await page.locator("#knowledge-failure").click();
  await page.waitForFunction(() => document.querySelector("#knowledge-status").textContent.includes("已观察到预期失败"), null, { timeout: 45000 });
  assert.match(await page.locator("#knowledge-output").textContent(), /deleted function/);
  await page.screenshot({ path: path.join(ARTIFACTS, "knowledge-failure-1440x1000.png") });
  await page.locator("#knowledge-repair").click();
  await page.waitForFunction(() => document.querySelector("#knowledge-status").textContent.includes("修复程序实际运行成功"), null, { timeout: 45000 });
  assert.match(await page.locator("#knowledge-output").textContent(), /\[run\] exit=0\n42/);
  await page.screenshot({ path: path.join(ARTIFACTS, "knowledge-repair-1440x1000.png") });
  await page.setViewportSize({ width: 390, height: 844 });
  assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
  await page.screenshot({ path: path.join(ARTIFACTS, "knowledge-mobile-390x844.png") });
  await page.locator("#knowledge-close").click();
  await page.setViewportSize({ width: 1440, height: 1000 });
  assert.equal(await editor.inputValue(), source);
  assert.equal(await page.locator("#check-result").textContent(), priorResult);
  assert.deepEqual(await (await page.request.get(baseUrl + "/api/v1/tasks/w1.calculator/history")).json(), history);
  // Real course error, not a fabricated response: the fixed calculator
  // fixture compiles the learner's actual file with a write through const.
  const bad = '#include "calculator.hpp"\nnamespace cppapplab::calculator {\nResult calculate(Operation, double, double) noexcept { const int n = 1; n = 2; return {}; }\n}\n';
  await editor.fill(bad);
  await clickWorkbenchAction(page, "#build-button");
  await page.waitForFunction(() => document.querySelector("#console-dock").dataset.state === "error", null, { timeout: 60000 });
  await page.locator('#diagnostics-list [data-knowledge-topic="const-reference"]').click();
  await page.waitForFunction(() => document.querySelector("#knowledge-topic-title").textContent.includes("const"));
  assert.match(await page.locator("#knowledge-explanation").textContent(), /引用/);
  await page.screenshot({ path: path.join(ARTIFACTS, "knowledge-real-error-1440x1000.png") });
  // The linked lesson is the current course; do not replace unsaved buffers.
  await page.locator("#knowledge-lesson").click();
  await page.waitForFunction(() => !document.querySelector("#knowledge-dialog").open);
  assert.equal(await editor.inputValue(), bad);
  await editor.fill(source);
  await clickWorkbenchAction(page, "#save-button");
  await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
  await page.locator("#console-toggle").click();
  report.knowledge = { actual_failure: true, actual_repair: true, real_course_error_link: true, course_history_unchanged: true };
}

async function initialLayout(page) {
  for (const dimensions of [{ width: 1440, height: 1000 }, { width: 1366, height: 768 },
    { width: 390, height: 844 }, { width: 320, height: 844 }]) {
    const metrics = await evidence(page, "idle", dimensions);
    if (!BASELINE) assertReadingSpace(metrics);
  }
  await page.setViewportSize({ width: 1440, height: 1000 });
  await resetReadingPosition(page);
  if (!BASELINE) {
    assert.equal(await page.locator("#start-button").isVisible(), false,
      "ready workspace status must not remain an actionable Start button");
    assert.equal(await page.locator("#workspace-state").isVisible(), true);
    assert.equal(await page.locator("#workspace-state").evaluate((element) => element.tagName), "SPAN");
    assert.equal(await page.locator("#workflow-action").isEnabled(), true);
    assert.equal(await page.locator("#workflow-action").getAttribute("data-action"), "build");
    assert.equal(await page.locator("#task-details").evaluate((element) => element.open), false);
    assert.equal(await page.locator("#lesson-content > h1:first-child").isVisible(), false,
      "the repeated lesson title stays in the document without consuming the first reading screen");
    await page.locator("#task-details summary").click();
    assert.equal(await page.locator("#task-subtitle").isVisible(), true,
      "technical course details must remain available on demand");
    await page.locator("#task-details summary").click();
    await page.locator("#lesson-language").selectOption("en");
    await resetReadingPosition(page);
    assertReadingSpace(await measurements(page));
    await page.locator("#lesson-language").selectOption("zh-CN");
    console.log("PASS: learning space / real prose and source are visible at desktop, short desktop, 390px and 320px");
  }
}

async function pendingBuild(page) {
  const pattern = "**/api/v1/workspaces/w1.calculator/build";
  let release;
  let entered;
  let completed;
  let routeStarted = false;
  const reached = new Promise((resolve) => { entered = resolve; });
  const gate = new Promise((resolve) => { release = resolve; });
  const handled = new Promise((resolve) => { completed = resolve; });
  const handler = async (route) => {
    routeStarted = true;
    entered();
    let error;
    try { await gate; await route.continue(); }
    catch (failure) { error = failure; }
    finally { completed(error); }
  };
  await page.route(pattern, handler);
  let captureError;
  try {
    await page.locator("#workflow-action").click();
    await waitWithin(reached, 10000, "The real Calculator build request must reach the held route");
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "执行中");
    if (!BASELINE) {
      // Check the first pending feedback without waiting for any color transition.
      const feedback = (await measurements(page)).nextAction;
      assert.equal(feedback.opacity, "1", "Pending next-action feedback is immediately opaque");
      assert.equal(feedback.backgroundImage, "none", "Pending next-action feedback uses its solid background");
      assert.ok(contrastRatio(feedback.color, feedback.background) >= 4.5,
        "The pending next-action label is immediately readable, before any transition wait: " + JSON.stringify(feedback));
    }
    const metrics = await evidence(page, "build-pending");
    if (!BASELINE) {
      assert.equal(metrics.dock.state, "running");
      assert.equal(metrics.dock.mode, "normal");
      assert.ok(metrics.dock.height >= 200 && metrics.dock.height <= 320);
      assert.equal(metrics.nextAction.hidden, false, "the clicked next-action control must remain in place");
      assert.equal(metrics.nextAction.disabled, true);
      assert.equal(metrics.nextAction.pending, "true");
      assert.equal(metrics.nextAction.busy, "true");
      assert.match(metrics.nextAction.text, /构建中/);
    }
    const before = await page.locator("#source-editor").inputValue();
    if (!(await page.locator("#console-dock").evaluate((element) => element.classList.contains("is-collapsed")))) {
      await page.locator("#console-toggle").click();
    }
    await page.waitForFunction(() => document.querySelector("#console-dock").classList.contains("is-collapsed"));
    assert.equal(await page.locator("#source-editor").inputValue(), before,
      "collapsing output during the operation must not change source code");
  } catch (error) {
    captureError = error;
    throw error;
  } finally {
    release();
    const cleanupErrors = [];
    if (routeStarted) {
      try {
        const error = await waitWithin(handled, 10000, "The released Calculator route must finish before interception is removed");
        if (error) cleanupErrors.push(error);
      } catch (error) { cleanupErrors.push(error); }
    }
    try { await page.unroute(pattern, handler); }
    catch (error) { cleanupErrors.push(error); }
    if (cleanupErrors.length) {
      throw new AggregateError(captureError ? [captureError, ...cleanupErrors] : cleanupErrors,
        "Calculator build capture cleanup failed" + (captureError ? " after the original capture failure" : ""));
    }
  }
  await page.waitForFunction(() => document.querySelector("#check-result").textContent === "构建成功" &&
    !document.querySelector('#course-list [data-task-id="w1.calculator"]').disabled, null, { timeout: 60000 });
  const metrics = await evidence(page, "build-success");
  assert.match(await page.locator("#console-output").textContent(), /Build result: PASS/);
  if (!BASELINE) {
    assert.equal(metrics.dock.state, "success");
    assert.equal(metrics.saveState, "已保存到本地工作区",
      "completed build restores the saved-file status rather than leaving a running message");
    assert.equal(metrics.dock.mode, "collapsed", "completion respects a learner's manual collapse while busy");
    assert.match(metrics.summary, /构建成功/);
    assert.equal(await page.locator("#console-summary").isVisible(), true);
    assert.equal(metrics.nextAction.hidden, false);
    assert.equal(metrics.nextAction.disabled, false);
    assert.equal(metrics.nextAction.action, "run", "a successful build offers the next real learning action");
    assert.notEqual(metrics.nextAction.pending, "true");
    console.log("PASS: learning space / next-action pending feedback, manual collapse and persistent real success");
  }
}

async function compilerFailure(page) {
  await page.locator("#source-editor").fill(INVALID_SOURCE);
  await page.waitForFunction(() => document.querySelector("#workflow-action").dataset.action === "build");
  await page.locator("#workflow-action").click();
  await page.waitForFunction(() => document.querySelector("#check-result").textContent === "构建失败" &&
    !document.querySelector('#course-list [data-task-id="w1.calculator"]').disabled, null, { timeout: 60000 });
  const output = await page.locator("#console-output").textContent();
  assert.match(output, /CPPAPPLAB_SPACE_ERROR/, "failure evidence comes from the real compiler");
  let metrics = await evidence(page, "compiler-error");
  if (!BASELINE) {
    assert.equal(metrics.dock.state, "error");
    assert.equal(metrics.saveState, "已保存到本地工作区",
      "compiler failure ends the running save-status label while keeping the source saved");
    assert.equal(metrics.dock.mode, "normal", "the next action exposes its output again");
    assert.match(metrics.summary, /CPPAPPLAB_SPACE_ERROR|error:/);
    assert.equal(metrics.nextAction.action, "troubleshooting");
    assert.equal(await page.locator("#diagnostics-list button").first().isVisible(), true);
  }
  if (!(await page.locator("#console-dock").evaluate((element) => element.classList.contains("is-collapsed")))) {
    await page.locator("#console-toggle").click();
  }
  metrics = await evidence(page, "error-collapsed");
  if (!BASELINE) {
    assert.equal(metrics.dock.mode, "collapsed");
    assert.equal(metrics.result, "构建失败");
    assert.match(metrics.summary, /CPPAPPLAB_SPACE_ERROR|error:/);
    assert.equal(await page.locator("#console-summary").isVisible(), true,
      "the first real compiler error must remain visible when the log is collapsed");
    const focus = await page.locator("#console-toggle").getAttribute("aria-expanded");
    assert.equal(focus, "false");
    await page.locator("#console-toggle").focus();
    await page.keyboard.press("Enter");
    await page.waitForFunction(() => document.querySelector("#console-dock").dataset.mode === "normal");
    await page.locator("#console-size").click();
    await page.waitForFunction(() => document.querySelector("#console-dock").dataset.mode === "large");
    const large = await measurements(page);
    assert.ok(large.dock.height >= 450 && large.dock.height <= 600,
      "manual enlargement provides space for detailed logs without becoming fullscreen: " + JSON.stringify(large.dock));
    assert.equal(await page.locator("#console-output").textContent(), output,
      "all terminal modes preserve the complete real output");
    await page.locator("#console-size").click();
    await page.waitForFunction(() => document.querySelector("#console-dock").dataset.mode === "normal");
    for (const width of [390, 320]) {
      const mobile = await evidence(page, "compiler-error", { width, height: 844 });
      assert.ok(mobile.dock.height >= 200 && mobile.dock.height <= 280,
        "mobile output uses a bounded readable area: " + JSON.stringify(mobile));
      assert.ok(mobile.dock.top >= 0 && mobile.dock.bottom <= 845 && mobile.pageWidth <= width + 1);
    }
    console.log("PASS: learning space / real compiler errors remain discoverable in normal, collapsed and enlarged terminal modes");
  }
}

async function main() {
  assert.ok(BINARY && fs.existsSync(BINARY), "CPPAPPLAB_TEST_BINARY must identify the real CLI");
  fs.mkdirSync(ARTIFACTS, { recursive: true });
  const browser = await chromium.launch({ headless: true,
    ...(process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH ? { executablePath: process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH } : {}) });
  let environment;
  try {
    environment = await fixture(browser);
    await startCalculator(environment.page);
    const history = await (await environment.page.request.get(environment.baseUrl + "/api/v1/tasks/w1.hello-world/history")).json();
    assert.equal(history.count, 1, "Calculator must be unlocked by exactly one actual Hello acceptance");
    assert.equal(history.records[0].result, "PASS");
    await initialLayout(environment.page);
    await themeEvidence(environment.page);
    await pendingBuild(environment.page);
    await compilerFailure(environment.page);
    if (!BASELINE) {
      await environment.page.setViewportSize({ width: 1440, height: 1000 });
      await knowledgeEvidence(environment.page, environment.baseUrl);
    }
    const calculatorHistory = await (await environment.page.request.get(environment.baseUrl + "/api/v1/tasks/w1.calculator/history")).json();
    assert.equal(calculatorHistory.count, 0, "ordinary build/error interactions must not manufacture formal acceptance");
    assert.deepEqual(environment.pageErrors, [], "learning-space interactions must not throw browser errors");
    console.log("PASS: learning space / " + (BASELINE ? "untouched PR-base" : "changed application") + " real before/after evidence");
  } catch (error) {
    if (environment && !environment.page.isClosed()) {
      await environment.page.screenshot({ path: path.join(ARTIFACTS, "learning-space-" + (BASELINE ? "baseline" : "changed") + "-failure.png") }).catch(() => {});
    }
    throw error;
  } finally {
    fs.writeFileSync(path.join(ARTIFACTS, "learning-space-" + (BASELINE ? "before" : "after") + "-geometry.json"), JSON.stringify(report, null, 2) + "\n");
    if (environment) await environment.close();
    await browser.close();
  }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
