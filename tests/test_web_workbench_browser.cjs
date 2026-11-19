"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const net = require("node:net");
const { spawn } = require("node:child_process");
const { chromium } = require("playwright");

const ROOT = path.resolve(__dirname, "..");
const BINARY = process.env.CPPAPPLAB_TEST_BINARY;
const SERVER_SCRIPT = path.join(ROOT, "web", "server", "cppapplab_web.py");
const ARTIFACT_DIR = process.env.CPPAPPLAB_BROWSER_ARTIFACT_DIR ||
  path.join(os.tmpdir(), "cppapplab-browser-artifacts");

const HELLO_SOURCE = [
  "#include <iostream>",
  "int main() {",
  '    std::cout << "Hello, CppAppLab!\\n";',
  "    return 0;",
  "}",
  ""
].join("\n");

const CALCULATOR_INVALID = [
  '#include "calculator.hpp"',
  "namespace cppapplab::calculator {",
  "Result calculate(Operation, double, double) noexcept {",
  "    return missing_symbol;",
  "}",
  "}",
  ""
].join("\n");

const CALCULATOR_SOLUTION = [
  '#include "calculator.hpp"',
  "namespace cppapplab::calculator {",
  "Result calculate(Operation operation, double left, double right) noexcept {",
  "    switch (operation) {",
  "    case Operation::Add: return {left + right, Error::None};",
  "    case Operation::Subtract: return {left - right, Error::None};",
  "    case Operation::Multiply: return {left * right, Error::None};",
  "    case Operation::Divide:",
  "        if (right == 0.0) return {0.0, Error::DivisionByZero};",
  "        return {left / right, Error::None};",
  "    }",
  "    return {0.0, Error::InvalidOperation};",
  "}",
  "}  // namespace cppapplab::calculator",
  ""
].join("\n");

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function freePort() {
  const server = net.createServer();
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  const port = address.port;
  await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  return port;
}

async function startService(label) {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cppapplab-browser-" + label + "-"));
  const home = path.join(temporary, "home");
  const workspaceRoot = path.join(home, "cppapplab-work");
  const stateRoot = path.join(home, ".local", "share", "cppapplab");
  fs.mkdirSync(workspaceRoot, { recursive: true });
  fs.mkdirSync(stateRoot, { recursive: true });
  const port = await freePort();
  const baseUrl = "http://127.0.0.1:" + port;
  const child = spawn("python3", [
    SERVER_SCRIPT,
    "--root", ROOT,
    "--cppapplab", BINARY,
    "--host", "127.0.0.1",
    "--port", String(port),
    "--workspace-root", workspaceRoot
  ], {
    cwd: ROOT,
    env: {
      ...process.env,
      HOME: home,
      CPPAPPLAB_STATE_DIR: stateRoot
    },
    stdio: ["ignore", "pipe", "pipe"]
  });

  let stdout = "";
  let stderr = "";
  child.stdout.setEncoding("utf8");
  child.stderr.setEncoding("utf8");
  child.stdout.on("data", (chunk) => { stdout += chunk; });
  child.stderr.on("data", (chunk) => { stderr += chunk; });

  const deadline = Date.now() + 30000;
  let ready = false;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error("CppAppLab web service exited during startup.\n" + stdout + "\n" + stderr);
    }
    try {
      const response = await fetch(baseUrl + "/api/v1/health");
      if (response.ok) {
        const health = await response.json();
        if (health.status === "ok" && health.task_count === 23) {
          ready = true;
          break;
        }
      }
    } catch {
      // The service has not bound its loopback port yet.
    }
    await sleep(250);
  }
  if (!ready) {
    child.kill("SIGKILL");
    throw new Error("CppAppLab web service did not become ready.\n" + stdout + "\n" + stderr);
  }

  return {
    temporary,
    child,
    baseUrl,
    async stop() {
      if (child.exitCode === null) {
        child.kill("SIGTERM");
        await Promise.race([
          new Promise((resolve) => child.once("exit", resolve)),
          sleep(5000).then(() => {
            if (child.exitCode === null) child.kill("SIGKILL");
          })
        ]);
      }
      fs.rmSync(temporary, { recursive: true, force: true });
    }
  };
}

async function waitForText(locator, expected, label, timeout = 30000) {
  await locator.page().waitForFunction(
    ({ selector, value }) => {
      const element = document.querySelector(selector);
      return element && element.textContent.trim() === value;
    },
    { selector: await locator.evaluate((element) => {
      if (!element.id) throw new Error("waitForText requires an element with an ID.");
      return "#" + element.id;
    }), value: expected },
    { timeout }
  ).catch(async (error) => {
    const actual = await locator.textContent().catch(() => "(missing)");
    throw new Error(label + ": expected \"" + expected + "\", received \"" + actual + "\". " + error.message);
  });
}

async function replaceMonacoEditor(page, value) {
  // A repeated course click can retain its breadcrumb while the editor reloads.
  // Wait for navigation/file readiness, while allowing intentional read-only tests.
  await page.waitForFunction(() => {
    const course = document.querySelector("#course-list .task-link.active");
    const tab = document.querySelector("#file-list .file-tab.active");
    const file = document.querySelector("#active-file")?.textContent.trim();
    return course && !course.disabled && tab && !tab.disabled && file && file !== "未选择文件" &&
      tab.title === file && course.dataset.taskId === document.querySelector("#task-breadcrumb")?.textContent.trim() &&
      window.monaco?.editor && document.querySelector("#editor-engine-status")?.textContent.startsWith("Monaco ") &&
      document.querySelector("#monaco-editor .view-lines");
  });
  const editorLines = page.locator("#monaco-editor .view-lines");
  await editorLines.click({ position: { x: 120, y: 10 } });
  await page.keyboard.press("Control+A");
  await page.keyboard.insertText(value);
}

async function readWorkspaceFile(page, baseUrl, taskId, relativePath) {
  const encoded = relativePath.split("/").map(encodeURIComponent).join("/");
  const response = await page.request.get(
    baseUrl + "/api/v1/workspaces/" + encodeURIComponent(taskId) + "/files/" + encoded
  );
  assert.equal(response.status(), 200, "workspace source GET must succeed");
  return response.json();
}

async function writeWorkspaceFile(page, baseUrl, taskId, relativePath, sourceContent) {
  const sessionResponse = await page.request.get(baseUrl + "/api/v1/session");
  assert.equal(sessionResponse.status(), 200, "workbench session must be available");
  const session = await sessionResponse.json();
  const encoded = relativePath.split("/").map(encodeURIComponent).join("/");
  const response = await page.request.put(
    baseUrl + "/api/v1/workspaces/" + encodeURIComponent(taskId) + "/files/" + encoded,
    {
      data: { content: sourceContent },
      headers: { Origin: baseUrl, "X-CppAppLab-Token": session.csrf_token }
    }
  );
  if (response.status() !== 200) {
    throw new Error("Could not stage reference source " + taskId + "/" + relativePath + ": " + await response.text());
  }
  assert.equal((await response.json()).saved, true);
}

async function copyReferenceSources(page, baseUrl, taskId, referenceRoot, relativePaths) {
  for (const relativePath of relativePaths) {
    const sourcePath = path.join(ROOT, referenceRoot, relativePath);
    assert.ok(fs.existsSync(sourcePath), "missing reference source: " + sourcePath);
    await writeWorkspaceFile(page, baseUrl, taskId, relativePath, fs.readFileSync(sourcePath, "utf8"));
  }
}

async function runApplicationAction(page, action, fields, expectedOutput) {
  await openApplicationRunner(page);
  await page.locator("#app-action").selectOption(action);
  for (const [name, value] of Object.entries(fields || {})) {
    await page.locator('[data-field="' + name + '"]').fill(value);
  }
  await clickWorkbenchAction(page, "#app-execute-button");
  await page.waitForFunction(
    () => document.querySelector("#check-result")?.textContent === "程序运行成功",
    null,
    { timeout: 15000 }
  );
  const output = await page.locator("#console-output").textContent();
  assert.ok(output.includes(expectedOutput), "unexpected " + action + " output: " + output);
  return output;
}

async function verifyReadOnlyReference(page, relativePath) {
  await page.waitForFunction(() => {
    const tab = document.querySelector("#file-list .file-tab");
    return tab && !tab.disabled && document.querySelector("#active-file").textContent !== "未选择文件";
  });
  const originalFile = await page.locator("#active-file").textContent();
  await page.locator('#file-list button[title="' + relativePath + '"]').click();
  await page.waitForFunction((file) => document.querySelector("#active-file").textContent === file, relativePath);
  const before = await page.evaluate((file) => window.monaco.editor.getModels()
    .find((model) => model.uri.path.endsWith("/" + file)).getValue(), relativePath);
  await replaceMonacoEditor(page, "must not change the supplied interface");
  const after = await page.evaluate((file) => window.monaco.editor.getModels()
    .find((model) => model.uri.path.endsWith("/" + file)).getValue(), relativePath);
  assert.equal(after, before, "Monaco must keep supplied reference headers read-only");
  assert.match(await page.locator("#save-state").textContent(), /只读/);
  await page.locator('#file-list button[title="' + originalFile + '"]').click();
  await page.waitForFunction((file) => document.querySelector("#active-file").textContent === file, originalFile);
}

async function openApplicationRunner(page) {
  const group = page.locator("#app-runner-details");
  if (!(await group.evaluate((element) => element.open))) {
    await group.locator("summary").click();
  }
}

async function clickWorkbenchAction(page, selector) {
  const isApplicationAction = selector === "#app-build-button" || selector === "#app-execute-button";
  const group = isApplicationAction ? page.locator("#app-runner-details") : page.locator("#editor-actions-group");
  if (!(await group.evaluate((element) => element.open))) {
    await group.locator("summary").click();
  }
  await page.locator(selector).click();
}

async function main() {
  assert.ok(BINARY && fs.existsSync(BINARY), "CPPAPPLAB_TEST_BINARY must point to the built learning engine");
  fs.mkdirSync(ARTIFACT_DIR, { recursive: true });

  const service = await startService("monaco");
  let fallbackService = null;
  let browser = null;
  const pageErrors = [];

  try {
    browser = await chromium.launch({
      headless: true,
      args: ["--no-sandbox", "--disable-dev-shm-usage"]
    });

    const context = await browser.newContext({
      viewport: { width: 1440, height: 1000 },
      deviceScaleFactor: 1
    });
    const page = await context.newPage();
    page.on("pageerror", (error) => pageErrors.push(error.message));

    await page.goto(service.baseUrl + "/", { waitUntil: "domcontentloaded" });
    await page.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    await page.waitForFunction(() =>
      document.querySelectorAll("#course-list .task-link").length === 23
    );

    // Exercise the actual first-use path rather than selecting a lesson from
    // the sidebar. The guide must return focus to its welcome-page opener and
    // preserve the welcome screen before the learner continues.
    assert.equal(await page.locator("#welcome-panel").isVisible(), true);
    assert.equal(await page.locator("#workbench-panel").isVisible(), false);
    await page.locator("#welcome-guide").click();
    await page.waitForFunction(() =>
      document.querySelector("#learning-guide")?.open === true &&
      document.querySelector("#guide-content")?.textContent.includes("第一次使用：先完成一个最小任务")
    );
    assert.equal(await page.locator('[data-guide-section="第一次使用"]').isEnabled(), true);
    await page.keyboard.press("Escape");
    await page.waitForFunction(() => document.querySelector("#learning-guide")?.open === false);
    assert.equal(await page.locator("#welcome-panel").isVisible(), true);
    assert.equal(await page.evaluate(() => document.activeElement?.id), "welcome-guide");

    const continueButton = page.locator("#continue-button");
    assert.equal(await continueButton.isEnabled(), true,
      "A fresh learner with a loaded course catalog must be able to continue");
    await continueButton.click();
    await page.waitForFunction(() =>
      !document.querySelector("#workbench-panel")?.hidden &&
      document.querySelector("#task-title")?.textContent.includes("First C++ Program")
    );

    const helloTask = page.locator('#course-list .task-link[data-task-id="w1.hello-world"]');
    assert.equal(
      await helloTask.count(),
      1,
      "W1 Hello World task must appear in the course list. Rendered list: " +
        await page.locator("#course-list").innerText()
    );
    await page.waitForFunction(() =>
      document.querySelector("#task-title")?.textContent.includes("First C++ Program")
    );
    const startButton = page.locator("#start-button");
    await startButton.click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    assert.equal(await startButton.getAttribute("aria-busy"), "false",
      "Start action clears its pending state after the workspace is ready");
    assert.equal(await startButton.getAttribute("data-pending"), null,
      "Start action returns to its normal button state after completion");
    await page.waitForFunction(() => {
      const value = document.querySelector("#editor-engine-status")?.textContent || "";
      return value.startsWith("Monaco ") || value === "基础编辑器（离线）";
    }, null, { timeout: 45000 });

    const editorMode = await page.locator("#editor-engine-status").textContent();
    assert.match(editorMode, /^Monaco /, "Primary browser acceptance requires the pinned Monaco editor to load");

    await replaceMonacoEditor(page, HELLO_SOURCE);
    await page.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent.includes("自动保存将在片刻后执行")
    );
    await page.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent === "已保存到本地工作区",
      null,
      { timeout: 10000 }
    );
    const helloSavedContent = await page.evaluate(async () => {
      const response = await fetch("/api/v1/workspaces/w1.hello-world/files/src/main.cpp", {
        cache: "no-store"
      });
      if (!response.ok) throw new Error("Could not read the saved Hello source.");
      return (await response.json()).content;
    });
    const normalizedLines = (source) =>
      source.replace(/\r\n/g, "\n").split("\n").map((line) => line.trim()).filter(Boolean);
    assert.deepEqual(
      normalizedLines(helloSavedContent),
      normalizedLines(HELLO_SOURCE),
      "Monaco autosave must persist every source line (indentation may be normalized by the editor)"
    );

    await clickWorkbenchAction(page, "#build-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 45000 });
    assert.match(await page.locator("#console-output").textContent(), /Build result: PASS/);

    await clickWorkbenchAction(page, "#run-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "执行成功", null, { timeout: 15000 });
    assert.match(await page.locator("#console-output").textContent(), /Hello, CppAppLab!/);
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "Build and Run must not mark the task as formally passed");

    await clickWorkbenchAction(page, "#check-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "PASS", null, { timeout: 45000 });
    await page.locator("#history-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#history-summary")?.textContent.includes("1 次正式验收")
    );
    assert.equal(await page.locator("#history-list .history-record").count(), 1);
    assert.equal(await page.locator("#history-list .history-pass").textContent(), "通过");
    assert.match(await page.locator("#history-list").textContent(), /源码指纹/);
    assert.match(await page.locator("#history-list").textContent(), /Workspace|cppapplab-work/);
    await page.locator("#history-close").click();
    assert.equal(await page.locator("#history-panel").isVisible(), false);

    // Keyboard-only access to the history control: tab focus, open with Enter,
    // then close with Escape without using a pointer.
    await page.locator("#history-button").focus();
    await page.keyboard.press("Enter");
    await page.waitForFunction(() => !document.querySelector("#history-panel")?.hidden);
    await page.keyboard.press("Escape");
    await page.waitForFunction(() => document.querySelector("#history-panel")?.hidden);
    assert.equal(await page.locator("#task-state").textContent(), "已通过",
      "Keyboard history navigation must not alter formal task state");

    await page.waitForFunction(() => {
      const button = Array.from(document.querySelectorAll("#course-list .task-link"))
        .find((item) => item.textContent.includes("Multi-file Calculator"));
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    const calculatorTask = page.locator('#course-list .task-link[data-task-id="w1.calculator"]');
    await calculatorTask.click();
    await page.waitForFunction(() =>
      document.querySelector("#task-title")?.textContent.includes("Multi-file Calculator")
    );
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    const fileTabs = page.locator("#file-list .file-tab");
    assert.equal(await fileTabs.count(), 3, "Calculator task must expose all three declared files");
    await page.waitForFunction(() =>
      document.querySelector("#active-file")?.textContent.trim() === "include/calculator.hpp"
    );

    await fileTabs.filter({ hasText: "src/main.cpp" }).click();
    await page.waitForFunction(() =>
      document.querySelector("#active-file")?.textContent.trim() === "src/main.cpp"
    );
    await fileTabs.filter({ hasText: "include/calculator.hpp" }).click();
    await page.waitForFunction(() =>
      document.querySelector("#active-file")?.textContent.trim() === "include/calculator.hpp"
    );
    await fileTabs.filter({ hasText: "src/calculator.cpp" }).click();
    await page.waitForFunction(() =>
      document.querySelector("#active-file")?.textContent.trim() === "src/calculator.cpp"
    );
    await page.waitForFunction(() => window.monaco && window.monaco.editor.getModels().length === 3);

    await replaceMonacoEditor(page, CALCULATOR_INVALID);
    await page.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.calculator/files/src/calculator.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("missing_symbol");
    }, null, { timeout: 10000 });

    await clickWorkbenchAction(page, "#build-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建失败", null, { timeout: 45000 });
    await page.waitForFunction(() =>
      !document.querySelector("#diagnostics-list")?.hidden &&
      document.querySelectorAll("#diagnostics-list .diagnostic-button").length > 0
    );
    const diagnosticText = await page.locator("#diagnostics-list").textContent();
    assert.match(diagnosticText, /src\/calculator\.cpp/, "Compiler diagnostics must map back to the learner source file");
    assert.match(diagnosticText, /错误/, "Compiler diagnostics must expose an error severity");

    await page.locator("#diagnostics-list .diagnostic-button").first().click();
    await page.waitForFunction(() =>
      document.activeElement?.classList?.contains("inputarea")
    );
    await page.waitForFunction(() => {
      const model = window.monaco.editor.getModels().find((item) => item.uri.path.endsWith("/src/calculator.cpp"));
      return model && window.monaco.editor.getModelMarkers({ resource: model.uri }).length > 0;
    });

    await replaceMonacoEditor(page, CALCULATOR_SOLUTION);
    await page.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.calculator/files/src/calculator.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("case Operation::Divide") &&
        !data.content.includes("missing_symbol");
    }, null, { timeout: 10000 });

    await clickWorkbenchAction(page, "#build-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 60000 });
    assert.match(await page.locator("#console-output").textContent(), /Build result: PASS/);

    await clickWorkbenchAction(page, "#run-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "执行成功", null, { timeout: 60000 });
    assert.match(await page.locator("#console-output").textContent(), /100% tests passed, 0 tests failed out of 9/);

    await clickWorkbenchAction(page, "#check-button");
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "PASS", null, { timeout: 60000 });
    await page.waitForFunction(() => {
      const button = Array.from(document.querySelectorAll("#course-list .task-link"))
        .find((item) => item.textContent.includes("Persistent Todo CLI"));
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    // The structured application profile is a separate learner-facing run path.
    // It must execute the current learner implementation without changing
    // the formal acceptance state.
    assert.equal(await page.locator("#app-runner").isVisible(), true);
    assert.equal(await page.locator("#app-profile-label").textContent(), "命令行计算器");
    await clickWorkbenchAction(page, "#app-build-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "应用构建成功",
      null,
      { timeout: 60000 }
    );
    await page.locator('[data-field="left"]').fill("20");
    await page.locator('[data-field="right"]').fill("22");
    await clickWorkbenchAction(page, "#app-execute-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "程序运行成功",
      null,
      { timeout: 15000 }
    );
    assert.match(await page.locator("#console-output").textContent(), /42/);
    assert.equal(await page.locator("#task-state").textContent(), "已通过",
      "Application build/run must not alter the formal acceptance result");

    // The released calculator lesson intentionally exercises division by zero.
    // Keep its diagnostic visible without calling the accepted source incorrect.
    const calculatorHistoryUrl = service.baseUrl + "/api/v1/tasks/w1.calculator/history";
    const calculatorHistory = await (await page.request.get(calculatorHistoryUrl)).json();
    await page.locator('[data-field="operation"]').selectOption("divide");
    await page.locator('[data-field="left"]').fill("1");
    await page.locator('[data-field="right"]').fill("0");
    await clickWorkbenchAction(page, "#app-execute-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "程序返回非零状态",
    null, { timeout: 15000 });
    assert.match(await page.locator("#console-output").textContent(), /error: division by zero/);
    assert.match(await page.locator("#check-result").getAttribute("class"), /\bfail\b/);
    assert.match(await page.locator("#app-runner-help").textContent(), /divide 1 0/);
    assert.doesNotMatch(await page.locator("#app-runner-help").textContent() + await page.locator("#toast").textContent(),
      /请修改代码|请修复代码|修改代码后重试/);
    assert.equal(await page.locator("#task-state").getAttribute("data-state"), "PASSED");
    assert.deepEqual(await (await page.request.get(calculatorHistoryUrl)).json(), calculatorHistory,
      "the documented negative application run must not change formal state or history");

    await page.locator("#editor-actions-group").evaluate((element) => { element.open = false; });
    await page.locator("#app-runner-details").evaluate((element) => { element.open = false; });

    const editorBoxDesktop = await page.locator(".editor-panel").boundingBox();
    const lessonBoxDesktop = await page.locator(".lesson-panel").boundingBox();
    assert.ok(editorBoxDesktop && lessonBoxDesktop);
    assert.ok(editorBoxDesktop.x < lessonBoxDesktop.x,
      "Desktop places the active code editor to the left of contextual lesson guidance");
    assert.ok(lessonBoxDesktop.x - (editorBoxDesktop.x + editorBoxDesktop.width) >= 20,
      "Desktop workbench keeps a visible gap between editor and lesson panels");
    assert.equal(await page.locator("#editor-actions-group").evaluate((element) => element.open), false,
      "Workspace actions are grouped and collapsed by default");
    assert.equal(await page.locator("#app-runner-details").evaluate((element) => element.open), false,
      "Application controls are grouped and collapsed by default");

    // Avoid distorted full-page evidence from a sticky header captured while
    // the browser is scrolled to an editor control.
    await page.locator('.main-content').evaluate((element) => { element.scrollTop = 0; });
    await page.waitForFunction(() => document.querySelector('.main-content')?.scrollTop === 0);
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "cppapplab-workbench-desktop.png"),
      fullPage: true
    });
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-1-layout-after.png"),
      fullPage: true
    });

    await page.locator('.main-content').evaluate((element) => { element.scrollTop = 700; });
    await page.waitForFunction(() => document.querySelector('.main-content')?.scrollTop > 0);
    const consoleDock = page.locator(".console-dock");
    const consoleGeometry = await consoleDock.evaluate((element) => {
      const rect = element.getBoundingClientRect();
      return {
        position: getComputedStyle(element).position,
        top: rect.top,
        bottom: rect.bottom,
        viewportHeight: window.innerHeight
      };
    });
    const consoleOutputGeometry = await page.locator("#console-output").evaluate((element) => {
      const rect = element.getBoundingClientRect();
      const style = getComputedStyle(element);
      return {
        height: rect.height,
        fontSize: parseFloat(style.fontSize),
        lineHeight: parseFloat(style.lineHeight)
      };
    });
    assert.equal(consoleGeometry.position, "fixed");
    assert.ok(consoleGeometry.top >= 0 && consoleGeometry.bottom <= consoleGeometry.viewportHeight,
      "The execution console remains inside the viewport after page scrolling: " +
      JSON.stringify(consoleGeometry));
    assert.ok(consoleOutputGeometry.height >= 360,
      "The default terminal viewport has room for at least twenty output lines: " +
      JSON.stringify(consoleOutputGeometry));
    assert.ok(consoleOutputGeometry.fontSize >= 12 && consoleOutputGeometry.lineHeight >= 17,
      "The terminal keeps readable output text rather than shrinking its font");
    assert.ok(Math.floor((consoleOutputGeometry.height - 24) / consoleOutputGeometry.lineHeight) >= 20,
      "The terminal's visible text area can show twenty lines without scrolling");

    // Compact mode must keep the terminal title/status dock visible while
    // returning the reserved viewport to the lesson and editor.
    const terminalToggle = page.locator("#console-toggle");
    await terminalToggle.click();
    await page.waitForFunction(() => {
      const dock = document.querySelector(".console-dock");
      return dock?.classList.contains("is-collapsed") &&
        document.querySelector("#console-toggle")?.getAttribute("aria-expanded") === "false" &&
        document.querySelector("#console-toggle")?.textContent.includes("展开终端");
    });
    const compactDockHeight = await consoleDock.evaluate((element) => element.getBoundingClientRect().height);
    assert.ok(compactDockHeight <= 56, "Compact terminal retains a visible header without occupying the output area");
    assert.equal(await page.locator("#console-title").isVisible(), true);
    assert.equal(await page.locator("#check-result").isVisible(), true);
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-2-console-compact-after.png"),
      fullPage: false
    });
    await terminalToggle.click();
    await page.waitForFunction(() => !document.querySelector(".console-dock")?.classList.contains("is-collapsed"));
    assert.ok(await consoleDock.evaluate((element) => element.getBoundingClientRect().height) >= 400,
      "Expanding the terminal restores the full output viewport");

    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-2-console-after.png"),
      fullPage: false
    });

    await page.locator("#editor-actions-group summary").click();
    const saveButton = page.locator("#save-button");
    const disabledStyle = await saveButton.evaluate((element) => {
      const style = getComputedStyle(element);
      return {
        disabled: element.disabled,
        opacity: style.opacity,
        background: style.backgroundColor,
        color: style.color,
        border: style.borderColor
      };
    });
    assert.equal(disabledStyle.disabled, true, "Save is unavailable when no file has unsaved edits");
    assert.equal(disabledStyle.opacity, "1", "Disabled controls stay fully opaque and legible");
    assert.notEqual(disabledStyle.background, "rgba(0, 0, 0, 0)",
      "Disabled controls have an explicit surface, not only reduced opacity");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-3-buttons-disabled-after.png"),
      fullPage: false
    });

    const buildButton = page.locator("#build-button");
    const originalBuildLabel = (await buildButton.textContent()).trim();
    const idleStyle = await buildButton.evaluate((element) => getComputedStyle(element).backgroundColor);
    await buildButton.hover();
    await page.waitForTimeout(180);
    const hoverStyle = await buildButton.evaluate((element) => {
      const style = getComputedStyle(element);
      return { background: style.backgroundColor, transform: style.transform };
    });
    assert.notEqual(hoverStyle.background, idleStyle, "Hover has a perceptible surface change");
    assert.notEqual(hoverStyle.transform, "none", "Hover has a small lift response");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-3-buttons-hover-after.png"),
      fullPage: false
    });

    let releaseBuildRequest;
    let buildRequestCount = 0;
    const buildRoute = "**/api/v1/workspaces/w1.calculator/build";
    await page.route(buildRoute, async (route) => {
      buildRequestCount += 1;
      await new Promise((resolve) => { releaseBuildRequest = resolve; });
      await route.continue();
    });

    await page.mouse.down();
    await page.waitForTimeout(180);
    const activeStyle = await buildButton.evaluate((element) => {
      const style = getComputedStyle(element);
      return { background: style.backgroundColor, transform: style.transform };
    });
    assert.notEqual(activeStyle.background, hoverStyle.background, "Active has a distinct pressed color");
    assert.notEqual(activeStyle.transform, hoverStyle.transform, "Active visibly presses the button down");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-3-buttons-active-after.png"),
      fullPage: false
    });
    await page.mouse.up();

    await page.waitForFunction(() => {
      const button = document.querySelector("#build-button");
      return button?.dataset.pending === "true" &&
        button.disabled &&
        button.getAttribute("aria-busy") === "true" &&
        button.textContent.includes("构建中");
    }, null, { timeout: 15000 });
    for (let attempt = 0; attempt < 50 && !releaseBuildRequest; attempt += 1) {
      await page.waitForTimeout(20);
    }
    assert.equal(typeof releaseBuildRequest, "function", "The real build request reached the held route");
    assert.equal(buildRequestCount, 1, "Exactly one real build request is pending");
    const formalDisabledStyle = await page.locator("#check-button").evaluate((element) => {
      const style = getComputedStyle(element);
      return {
        disabled: element.disabled,
        opacity: style.opacity,
        color: style.color,
        border: style.borderColor,
        backgroundImage: style.backgroundImage
      };
    });
    assert.equal(formalDisabledStyle.disabled, true, "Formal acceptance is unavailable while another action runs");
    assert.equal(formalDisabledStyle.opacity, "1", "Disabled primary action remains fully opaque");
    assert.notEqual(formalDisabledStyle.color, "rgba(0, 0, 0, 0)",
      "Disabled formal-acceptance label remains visible");
    assert.notEqual(formalDisabledStyle.border, "rgba(0, 0, 0, 0)",
      "Disabled formal-acceptance control retains a visible boundary");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "stage-3-buttons-pending-after.png"),
      fullPage: false
    });

    await page.evaluate(() => {
      document.querySelector("#build-button").dispatchEvent(
        new MouseEvent("click", { bubbles: true })
      );
    });
    await page.waitForTimeout(100);
    assert.equal(buildRequestCount, 1, "A second click while pending cannot start another build");
    assert.equal(typeof releaseBuildRequest, "function");
    releaseBuildRequest();
    await page.waitForFunction(
      () => document.querySelector("#check-result")?.textContent === "构建成功",
      null,
      { timeout: 60000 }
    );
    await page.unroute(buildRoute);
    assert.equal(await buildButton.getAttribute("data-pending"), null);
    assert.equal(await buildButton.getAttribute("aria-busy"), "false");
    assert.equal((await buildButton.textContent()).trim(), originalBuildLabel,
      "The original task-specific action label is restored after completion");
    await page.locator("#editor-actions-group summary").click();

    await page.setViewportSize({ width: 390, height: 844 });
    await page.locator('.main-content').evaluate((element) => { element.scrollTop = 0; });
    await page.waitForFunction(() => document.querySelector('.main-content')?.scrollTop === 0);
    await page.waitForTimeout(200);
    const mobileOverflow = await page.evaluate(() =>
      document.documentElement.scrollWidth > document.documentElement.clientWidth + 1
    );
    assert.equal(mobileOverflow, false, "Mobile workbench must not introduce horizontal page overflow");
    const lessonBoxMobile = await page.locator(".lesson-panel").boundingBox();
    const editorBoxMobile = await page.locator(".editor-panel").boundingBox();
    assert.ok(lessonBoxMobile && editorBoxMobile);
    assert.ok(lessonBoxMobile.y < editorBoxMobile.y,
      "Mobile retains the instructional sequence: lesson before source editor");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "cppapplab-workbench-mobile.png"),
      fullPage: true
    });

    // W2 Todo is unlocked by the real W1 calculator acceptance above.
    // First verify that the incomplete learner implementation surfaces its TODO.
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.locator('#course-list .task-link[data-task-id="w2.todo-cli"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w2.todo-cli"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "持久化 Todo CLI");
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await verifyReadOnlyReference(page, "include/todo.hpp");
    await clickWorkbenchAction(page, "#app-build-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "应用构建成功",
      null, { timeout: 60000 }
    );
    await openApplicationRunner(page);
    await page.locator("#app-action").selectOption("list");
    await clickWorkbenchAction(page, "#app-execute-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "程序返回非零状态",
      null, { timeout: 15000 }
    );
    assert.match(await page.locator("#console-output").textContent(), /not implemented/);
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "Failed Todo demo must not write formal acceptance evidence");

    // The browser test harness stages known-good sources only into this disposable
    // CI workspace. The product never pre-fills or replaces learner code.
    await copyReferenceSources(
      page, service.baseUrl, "w2.todo-cli", "labs/w2/todo/reference", ["src/todo.cpp"]
    );
    await page.locator('#course-list .task-link[data-task-id="w2.todo-cli"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w2.todo-cli" &&
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await clickWorkbenchAction(page, "#app-build-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "应用构建成功",
      null, { timeout: 60000 }
    );
    await runApplicationAction(page, "add", { title: "browser todo" }, "Added #1: browser todo");
    await runApplicationAction(page, "list", {}, "#1 [ ] browser todo");
    await runApplicationAction(page, "done", { id: "1" }, "Completed #1");
    await runApplicationAction(page, "list", {}, "#1 [x] browser todo");
    await runApplicationAction(page, "remove", { id: "1" }, "Removed #1");
    await runApplicationAction(page, "list", {}, "No tasks.");
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "Todo application CRUD must not write formal acceptance evidence");

    await clickWorkbenchAction(page, "#check-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "PASS",
      null, { timeout: 60000 }
    );
    await page.waitForFunction(() => {
      const button = document.querySelector('#course-list .task-link[data-task-id="w3.task-manager"]');
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    // W3 TaskManager runs its real application profile in the browser.
    // Reload after adding a task to prove workspace-local persistence.
    await page.locator('#course-list .task-link[data-task-id="w3.task-manager"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w3.task-manager"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "TaskManager CLI");
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await verifyReadOnlyReference(page, "include/task.hpp");
    await verifyReadOnlyReference(page, "include/task_manager.hpp");
    await copyReferenceSources(
      page, service.baseUrl, "w3.task-manager", "labs/w3/task-manager/reference",
      ["src/task.cpp", "src/task_manager.cpp"]
    );
    await page.locator('#course-list .task-link[data-task-id="w3.task-manager"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w3.task-manager" &&
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await clickWorkbenchAction(page, "#app-build-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "应用构建成功",
      null, { timeout: 60000 }
    );
    await runApplicationAction(page, "add", { title: "browser task" }, "Added #1: browser task");
    await runApplicationAction(page, "list", {}, "#1 [ ] browser task");

    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    await page.waitForFunction(() =>
      document.querySelectorAll("#course-list .task-link").length === 23
    );
    await page.locator('#course-list .task-link[data-task-id="w3.task-manager"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w3.task-manager" &&
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await runApplicationAction(page, "list", {}, "#1 [ ] browser task");
    await runApplicationAction(page, "done", { id: "1" }, "Completed #1");
    await runApplicationAction(page, "list", {}, "#1 [x] browser task");
    await runApplicationAction(page, "remove", { id: "1" }, "Removed #1");
    await runApplicationAction(page, "list", {}, "No tasks.");
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "TaskManager application CRUD must not write formal acceptance evidence");

    await clickWorkbenchAction(page, "#check-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "PASS",
      null, { timeout: 60000 }
    );
    await page.waitForFunction(() => {
      const button = document.querySelector('#course-list .task-link[data-task-id="w4.quality-gate"]');
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    // W4 Quality Gate deliberately separates a complete TaskManager runtime
    // implementation from learner-authored quality tests that still contain TODOs.
    // The app should work; formal acceptance must still fail until tests are written.
    await page.locator('#course-list .task-link[data-task-id="w4.quality-gate"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w4.quality-gate"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "TaskManager Quality Gate CLI");
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await clickWorkbenchAction(page, "#app-build-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "应用构建成功",
      null, { timeout: 60000 }
    );
    await runApplicationAction(page, "add", { title: "quality gate task" }, "Added #1: quality gate task");
    await runApplicationAction(page, "list", {}, "#1 [ ] quality gate task");
    await runApplicationAction(page, "done", { id: "1" }, "Completed #1");
    await runApplicationAction(page, "list", {}, "#1 [x] quality gate task");
    await runApplicationAction(page, "remove", { id: "1" }, "Removed #1");
    await runApplicationAction(page, "list", {}, "No tasks.");
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "Quality Gate application runs must not write formal acceptance evidence");

    await clickWorkbenchAction(page, "#check-button");
    await page.waitForFunction(() =>
      document.querySelector("#check-result")?.textContent === "未通过",
      null, { timeout: 60000 }
    );
    assert.match(await page.locator("#console-output").textContent(), /TODO\(W4-TEST-/);
    assert.equal(await page.locator("#task-state").textContent(), "需要修改",
      "Incomplete learner-authored quality tests must block formal acceptance");
    // Other one-shot and structured profiles expose finite browser schemas.
    // They remain non-executable until their prerequisites have passed.
    await page.locator('#course-list .task-link[data-task-id="w5.system-lab"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w5.system-lab"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "受限进程运行演示");
    assert.deepEqual(await page.locator("#app-action option").evaluateAll((items) =>
      items.map((item) => item.value)
    ), ["echo"]);
    assert.equal(await page.locator("#app-fields input, #app-fields textarea").count(), 0);
    assert.equal(await page.locator("#app-build-button").isDisabled(), true);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true);
    assert.match(await page.locator("#app-runner-help").textContent(), /前置课程/);

    await page.locator('#course-list .task-link[data-task-id="w6.thread-pool"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w6.thread-pool"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "Thread Pool Demo");
    assert.deepEqual(await page.locator("#app-action option").evaluateAll((items) =>
      items.map((item) => item.value)
    ), ["demo"]);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true);

    await page.locator('#course-list .task-link[data-task-id="w8.http-protocol"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w8.http-protocol"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "HTTP 协议消息");
    assert.deepEqual(await page.locator("#app-action option").evaluateAll((items) =>
      items.map((item) => item.value)
    ), ["parse-line", "response"]);
    await openApplicationRunner(page);
    await page.locator("#app-action").selectOption("parse-line");
    assert.equal(await page.locator('[data-field="request_line"]').count(), 1);
    await page.locator("#app-action").selectOption("response");
    assert.equal(await page.locator("#app-fields input, #app-fields textarea").count(), 0);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true);

    await page.locator('#course-list .task-link[data-task-id="w8.static-files"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w8.static-files"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "HTTP 静态文件服务");
    assert.deepEqual(await page.locator('[data-field="method"] option').evaluateAll((items) =>
      items.map((item) => item.value)
    ), ["GET", "HEAD", "POST"]);
    assert.equal(await page.locator('[data-field="target"]').count(), 1);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true);

    // Network-service tasks wait for P8-C3; do not fabricate one-shot runners.
    for (const taskId of ["w7.tcp-echo", "w7.tcp-chat", "w8.http-server"]) {
      await page.locator('#course-list .task-link[data-task-id="' + taskId + '"]').click();
      await page.waitForFunction((id) =>
        document.querySelector("#task-breadcrumb")?.textContent.trim() === id,
        taskId
      );
      assert.equal(await page.locator("#app-runner").isVisible(), false,
        taskId + " must wait for a real network-service profile");
      assert.equal(await page.locator("#service-runner").isVisible(), true,
        taskId + " must expose its registered network-service lifecycle panel");
      assert.equal(await page.locator("#service-start-button").isDisabled(), true,
        taskId + " must remain disabled until prerequisites and workspace are ready");
      assert.equal(await page.locator("#service-stop-button").isDisabled(), true);
      if (taskId === "w7.tcp-echo") {
        assert.equal(await page.locator("#service-echo-message").count(), 1);
        assert.equal(await page.locator("#service-echo-send").isDisabled(), true);
      } else if (taskId === "w7.tcp-chat") {
        assert.equal(await page.locator("#service-chat-nickname").count(), 1);
        assert.equal(await page.locator("#service-chat-connect").isDisabled(), true);
        assert.equal(await page.locator("#service-chat-client").count(), 1);
        assert.equal(await page.locator("#service-chat-message").count(), 1);
        assert.equal(await page.locator("#service-chat-output").count(), 1);
      } else if (taskId === "w8.http-server") {
        assert.equal(await page.locator("#service-http-method").count(), 1);
        assert.deepEqual(await page.locator("#service-http-method option").evaluateAll((items) =>
          items.map((item) => item.value)
        ), ["GET", "HEAD", "POST"]);
        assert.equal(await page.locator("#service-http-target").inputValue(), "/");
        assert.equal(await page.locator("#service-http-body").isDisabled(), true);
        assert.equal(await page.locator("#service-http-send").isDisabled(), true);
      }
    }

    // A locked later task may display its instructional action schema, but
    // must not permit launching an application before prerequisites pass.
    await page.locator('#course-list .task-link[data-task-id="w8.http-parser"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w8.http-parser"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "HTTP 请求解析器");
    assert.equal(await page.locator("#app-fields textarea").count(), 1);
    assert.match(await page.locator("#app-fields textarea").inputValue(), /Host: localhost/);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true,
      "Locked parser tasks must not be executed from the browser");
    assert.match(await page.locator("#app-runner-help").textContent(), /前置课程/);
    await page.locator('#course-list .task-link[data-task-id="w8.router"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w8.router"
    );
    assert.equal(await page.locator("#app-profile-label").textContent(), "HTTP 路由与分发");
    assert.equal(await page.locator("#app-fields textarea").count(), 1);
    assert.equal(await page.locator("#app-execute-button").isDisabled(), true);

    // P8-C5: walk every catalog entry in the actual browser. This verifies
    // that all 23 tasks are selectable, have their authoritative lesson and
    // state rendered, and expose the same prerequisite metadata as the API.
    const catalogTaskIds = [
      "w1.hello-world", "w1.calculator", "w2.todo-cli", "w3.task-manager",
      "w4.quality-gate", "w5.system-lab", "w6.thread-pool", "w7.tcp-echo",
      "w7.tcp-chat", "w8.http-protocol", "w8.http-parser", "w8.static-files",
      "w8.router", "w8.http-server", "w8.http-connection-policy",
      "w8.http-keep-alive", "w8.http-connection-guards",
      "w8.http-graceful-shutdown", "w8.http-observability",
      "w8.http-operational-load", "w9.http-chunked", "w9.request-target",
      "w9.static-root"
    ];
    assert.equal(await page.locator("#course-list .task-link").count(), catalogTaskIds.length);
    for (const taskId of catalogTaskIds) {
      const taskButton = page.locator('#course-list .task-link[data-task-id="' + taskId + '"]');
      assert.equal(await taskButton.count(), 1, taskId + " must exist exactly once in the browser catalog");
      await taskButton.click();
      await page.waitForFunction((id) =>
        document.querySelector("#task-breadcrumb")?.textContent.trim() === id,
        taskId
      );
      assert.ok((await page.locator("#task-title").textContent()).trim().length > 0,
        taskId + " must render a lesson title");
      assert.ok((await page.locator("#lesson-content").textContent()).trim().length > 0,
        taskId + " must render its authoritative lesson");
      assert.match(
        await page.locator("#lesson-content").textContent(),
        /Start here/,
        taskId + " must expose an immediate task-first learning path"
      );
      assert.ok(["已通过", "进行中", "需要修改", "结果已过期", "未开始", "可开始", "未解锁", "状态未知"].includes(
        await page.locator("#task-state").textContent()
      ), taskId + " must expose a known course state");
      const taskDetail = await page.request.get(service.baseUrl + "/api/v1/tasks/" + taskId);
      assert.equal(taskDetail.status(), 200, taskId + " detail API must be available");
      const detail = await taskDetail.json();
      assert.equal(detail.id, taskId);
      assert.ok(Array.isArray(detail.prerequisites));
      if (detail.state === "LOCKED") {
        assert.ok(detail.state_reason.length > 0,
          taskId + " must explain why its prerequisites are locked");
      }
    }

    // Keyboard-only course navigation: the first catalog item receives
    // focus, Tab advances to the next task, and Enter selects it.
    const firstCourseTask = page.locator("#course-list .task-link").first();
    const secondCourseTask = page.locator("#course-list .task-link").nth(1);
    await firstCourseTask.focus();
    await page.keyboard.press("Tab");
    assert.equal(
      await secondCourseTask.evaluate((element) => element === document.activeElement),
      true,
      "Tab must move between course tasks in catalog order"
    );
    const secondTaskId = await secondCourseTask.getAttribute("data-task-id");
    await page.keyboard.press("Enter");
    await page.waitForFunction((id) =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === id,
      secondTaskId
    );

    // First-time learner guidance must be present in the first four lessons.
    for (const taskId of [
      "w1.hello-world", "w1.calculator", "w2.todo-cli",
      "w3.task-manager", "w4.quality-gate"
    ]) {
      await page.locator('#course-list .task-link[data-task-id="' + taskId + '"]').click();
      await page.waitForFunction((id) =>
        document.querySelector("#task-breadcrumb")?.textContent.trim() === id,
        taskId
      );
      assert.match(
        await page.locator("#lesson-content").textContent(),
        /Start here/,
        taskId + " must provide a task-first beginner walkthrough"
      );
    }

    // Refresh must preserve the last formal PASS and the history timeline.
    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    await page.waitForFunction(() =>
      document.querySelectorAll("#course-list .task-link").length === 23
    );
    await page.locator('#course-list .task-link[data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w1.hello-world"
    );
    assert.equal(await page.locator("#task-state").textContent(), "已通过",
      "A browser refresh must not lose the authoritative formal PASS");
    await page.locator("#history-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#history-summary")?.textContent.includes("1 次正式验收")
    );
    assert.equal(await page.locator("#history-list .history-record").count(), 1);

    // Unsaved edits must trigger the browser's beforeunload guard. Cancel the
    // first reload, save explicitly, then reload and verify persisted content.
    const helloSourceBefore = await readWorkspaceFile(
      page, service.baseUrl, "w1.hello-world", "src/main.cpp"
    );
    const helloSourceEdited = helloSourceBefore.content.replace(
      "Hello, CppAppLab!", "Hello, CppAppLab after refresh!"
    );
    assert.notEqual(helloSourceEdited, helloSourceBefore.content);
    await page.locator('#course-list .task-link[data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w1.hello-world"
    );
    await replaceMonacoEditor(page, helloSourceEdited);
    await page.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent.includes("自动保存将在片刻后执行")
    );
    let unloadPromptSeen = false;
    page.once("dialog", async (dialog) => {
      unloadPromptSeen = dialog.type() === "beforeunload";
      await dialog.dismiss();
    });
    // A dismissed beforeunload dialog cancels navigation by design, so
    // Playwright's reload navigation wait must be bounded rather than waiting
    // for DOMContentLoaded that will never occur.
    await page.reload({ waitUntil: "domcontentloaded", timeout: 3000 }).catch(() => {});
    assert.equal(unloadPromptSeen, true,
      "Reload with dirty learner source must ask before discarding changes");
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w1.hello-world"
    );
    await page.keyboard.press("Control+S");
    await page.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent === "已保存到本地工作区",
      null, { timeout: 15000 }
    );
    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    const helloSourceAfterReload = await readWorkspaceFile(
      page, service.baseUrl, "w1.hello-world", "src/main.cpp"
    );
    const normalizeSourceIndentation = (source) =>
      source.split("\n").map((line) => line.trimStart()).join("\n").trimEnd();
    assert.equal(
      normalizeSourceIndentation(helloSourceAfterReload.content),
      normalizeSourceIndentation(helloSourceEdited),
      "Explicitly saved source must survive a browser reload (editor auto-indent may normalize leading whitespace)"
    );
    await page.locator('#course-list .task-link[data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent.trim() === "w1.hello-world" &&
      document.querySelector("#task-state")?.textContent === "结果已过期"
    );
    assert.match(
      await page.locator("#task-subtitle").textContent(),
      /无前置任务/,
      "The selected task must show authoritative prerequisite context after recovery"
    );
    assert.equal(
      await page.locator("#task-state").textContent(),
      "结果已过期",
      "A source edit after a historical PASS must never preserve a current PASS after reload"
    );
    await page.locator('#course-list .task-link[data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-state")?.textContent === "结果已过期"
    );

    // Monaco may reject an in-flight worker request with its documented "Canceled"
    // sentinel when an editor model is disposed during task switching. It is not an
    // application exception; all other uncaught page errors remain test failures.
    const unexpectedPageErrors = pageErrors.filter((message) => message !== "Canceled");
    assert.deepEqual(
      unexpectedPageErrors,
      [],
      "The Monaco workbench must not emit uncaught application exceptions"
    );
    await context.close();

    fallbackService = await startService("offline");
    const fallbackContext = await browser.newContext({
      viewport: { width: 1280, height: 900 }
    });
    const fallbackPage = await fallbackContext.newPage();
    // Block the exact Monaco loader path on every configured CDN so the
    // offline branch is deterministic even if another host is reachable.
    await fallbackPage.route("**/vs/loader.js", (route) => route.abort());
    const fallbackErrors = [];
    fallbackPage.on("pageerror", (error) => fallbackErrors.push(error.message));

    await fallbackPage.goto(fallbackService.baseUrl + "/", { waitUntil: "domcontentloaded" });
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    await fallbackPage.waitForFunction(() =>
      document.querySelectorAll("#course-list .task-link").length === 23
    );
    const fallbackHello = fallbackPage.locator('#course-list .task-link[data-task-id="w1.hello-world"]');
    await fallbackHello.click();
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#task-title")?.textContent.includes("First C++ Program")
    );
    await fallbackPage.locator("#start-button").click();
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#editor-engine-status")?.textContent === "基础编辑器（离线）",
      null,
      { timeout: 15000 }
    );
    assert.equal(await fallbackPage.locator("#source-editor").isVisible(), true,
      "Offline fallback textarea must be visible");
    await fallbackPage.locator("#source-editor").fill(HELLO_SOURCE);
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent.includes("自动保存将在片刻后执行")
    );
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent === "已保存到本地工作区",
      null,
      { timeout: 10000 }
    );
    const fallbackSavedContent = await fallbackPage.evaluate(async () => {
      const response = await fetch("/api/v1/workspaces/w1.hello-world/files/src/main.cpp", {
        cache: "no-store"
      });
      if (!response.ok) throw new Error("Could not read the saved fallback source.");
      return (await response.json()).content;
    });
    assert.deepEqual(
      normalizedLines(fallbackSavedContent),
      normalizedLines(HELLO_SOURCE),
      "Textarea fallback autosave must persist every source line"
    );
    await clickWorkbenchAction(fallbackPage, "#build-button");
    await fallbackPage.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 45000 });
    await clickWorkbenchAction(fallbackPage, "#run-button");
    await fallbackPage.waitForFunction(() => document.querySelector("#check-result")?.textContent === "执行成功", null, { timeout: 15000 });
    assert.match(await fallbackPage.locator("#console-output").textContent(), /Hello, CppAppLab!/);
    assert.deepEqual(fallbackErrors, [], "The offline fallback must not emit uncaught browser exceptions");

    await fallbackContext.close();
    console.log("PASS: Monaco multi-file editing, autosave, build/run/evidence separation, CMake diagnostics, line navigation, task unlocking, mobile layout and offline editor fallback.");
  } finally {
    if (browser) await browser.close().catch(() => {});
    await service.stop().catch(() => {});
    if (fallbackService) await fallbackService.stop().catch(() => {});
  }
}

main().catch((error) => {
  console.error(error.stack || error);
  process.exitCode = 1;
});
