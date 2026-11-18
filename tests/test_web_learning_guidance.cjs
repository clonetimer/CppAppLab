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
const HELLO = '#include <iostream>\nint main() { std::cout << "Hello, CppAppLab!\\n"; }\n';
const ARTIFACTS = process.env.CPPAPPLAB_BROWSER_ARTIFACT_DIR || path.join(os.tmpdir(), "cppapplab-guidance-artifacts");
const EDITOR_CDN = /https?:\/\/(cdn\.jsdelivr\.net|gcore\.jsdelivr\.net|cdnjs\.cloudflare\.com)\//;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function waitWithin(promise, timeout, message) {
  let timer;
  const deadline = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(message)), timeout);
  });
  try {
    return await Promise.race([promise, deadline]);
  } finally {
    clearTimeout(timer);
  }
}

async function freePort() {
  const server = net.createServer();
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const port = server.address().port;
  await new Promise((resolve) => server.close(resolve));
  return port;
}

async function fixture(browser, { referenceFixture = false, pendingEditor = false } = {}) {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cppapplab-guidance-"));
  const root = path.join(temporary, "repo");
  const home = path.join(temporary, "home");
  const workspace = path.join(home, "cppapplab-work");
  const state = path.join(temporary, "state");
  fs.mkdirSync(root);
  fs.mkdirSync(workspace, { recursive: true });
  for (const directory of ["course", "labs", "web"]) {
    fs.cpSync(path.join(ROOT, directory), path.join(root, directory), { recursive: true });
  }
  if (referenceFixture) {
    // Isolate header-reading UI from the prerequisite journey. This modifies
    // only a temporary catalog; the existing full browser suite covers real
    // W2/W3 prerequisite acceptance with the unchanged released catalog.
    const catalog = path.join(root, "course", "tasks.tsv");
    const rows = fs.readFileSync(catalog, "utf8").split("\n").map((line) => {
      const fields = line.split("\t");
      if (fields[0] === "w3.task-manager") fields[12] = "";
      return fields.join("\t");
    });
    fs.writeFileSync(catalog, rows.join("\n"));
  }
  const baseUrl = "http://127.0.0.1:" + await freePort();
  const child = spawn("python3", [path.join(ROOT, "web", "server", "cppapplab_web.py"),
    "--root", root, "--cppapplab", BINARY, "--host", "127.0.0.1", "--port", baseUrl.split(":").pop(),
    "--workspace-root", workspace], {
    env: { ...process.env, HOME: home, CPPAPPLAB_STATE_DIR: state, PYTHONDONTWRITEBYTECODE: "1" },
    stdio: ["ignore", "ignore", "pipe"]
  });
  let errors = "";
  child.stderr.on("data", (data) => { errors += data; });
  const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
  let releaseEditorGate;
  const editorGate = new Promise((resolve) => { releaseEditorGate = resolve; });
  const editorRequests = new Set();
  // Most cases use the supported offline editor. Timeout/manual cases retain
  // real browser requests until the product has selected its fallback.
  await context.route(EDITOR_CDN, (route) => {
    const pending = (async () => {
      if (pendingEditor) await editorGate;
      await route.abort().catch(() => {});
    })();
    editorRequests.add(pending);
    pending.finally(() => editorRequests.delete(pending));
    return pending;
  });
  async function releaseEditorRequests() {
    releaseEditorGate();
    await waitWithin(Promise.all([...editorRequests]), 5000, "Timed out releasing editor requests");
  }
  async function close() {
    releaseEditorGate();
    try {
      await releaseEditorRequests();
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
  const page = await context.newPage();
  page.setDefaultTimeout(10000);
  try {
    let ready = false;
    for (let attempt = 0; attempt < 100; attempt += 1) {
      if (child.exitCode !== null) throw new Error(errors);
      try {
        if ((await fetch(baseUrl + "/api/v1/health")).ok) { ready = true; break; }
      } catch { /* wait for the loopback server */ }
      await sleep(50);
    }
    assert.ok(ready, "real learning server must start: " + errors);
    await page.goto(baseUrl);
    await page.waitForFunction(() => document.querySelectorAll("#course-list .task-link").length === 23);
  } catch (error) {
    await close();
    throw error;
  }
  return {
    page, root, workspace, baseUrl, close, releaseEditorRequests,
    pendingEditorRequests: () => editorRequests.size
  };
}

async function select(page, taskId) {
  await page.locator('#course-list [data-task-id="' + taskId + '"]').click();
  await page.waitForFunction((id) => document.querySelector("#task-breadcrumb").textContent === id &&
    !document.querySelector('#course-list [data-task-id="' + id + '"]').disabled, taskId);
}

async function position(page, taskId, filePath, content) {
  await page.waitForFunction(({ id, file }) => {
    const route = new URLSearchParams(location.hash.slice(1));
    return document.querySelector("#task-breadcrumb").textContent === id &&
      document.querySelector("#active-file").textContent === file &&
      !document.querySelector("#source-editor").disabled &&
      !document.querySelector('#course-list [data-task-id="' + id + '"]').disabled &&
      route.get("task") === id && route.get("file") === file;
  }, { id: taskId, file: filePath });
  if (content !== undefined) assert.equal(await page.locator("#source-editor").inputValue(), content);
}

async function saveSource(page, content) {
  await page.locator("#source-editor").fill(content);
  await page.locator("#save-button").click();
  await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
}

async function startHello(page) {
  await select(page, "w1.hello-world");
  await page.locator("#start-button").click();
  await page.waitForFunction(() => !document.querySelector("#source-editor").disabled);
  await page.locator("#source-editor").fill(HELLO);
  await page.locator("#save-button").click();
  await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
}

async function passHello(page) {
  await startHello(page);
  await page.locator("#check-button").click();
  // The PASS badge precedes the catalog refresh and release of the busy guard.
  await page.waitForFunction(() => {
    const calculator = document.querySelector('#course-list [data-task-id="w1.calculator"]');
    return document.querySelector("#check-result").textContent === "PASS" &&
      document.querySelector("#task-state").dataset.state === "PASSED" &&
      calculator && calculator.dataset.state === "AVAILABLE" && !calculator.disabled;
  });
}

async function startCalculator(page) {
  await passHello(page);
  await select(page, "w1.calculator");
  await page.locator("#start-button").click();
  await position(page, "w1.calculator", "include/calculator.hpp");
  const entries = await page.evaluate(() => history.length);
  await page.locator('#file-list button[title="src/calculator.cpp"]').click();
  await position(page, "w1.calculator", "src/calculator.cpp");
  assert.equal(await page.evaluate(() => history.length), entries, "file selection replaces the current history entry");
}

async function guideReady(page) {
  await page.waitForFunction(() => document.querySelector("#learning-guide").open &&
    !document.querySelector("#guide-content").hidden &&
    document.querySelector("#guide-content").getAttribute("aria-busy") === "false" &&
    document.querySelectorAll("#guide-content .guide-task-button").length === 23 &&
    [...document.querySelectorAll("#guide-sections button")].every((button) => !button.disabled));
}

async function guideSection(page, section) {
  await page.locator('#guide-sections [data-guide-section="' + section + '"]').focus();
  await page.keyboard.press("Enter");
  await page.waitForFunction((section) => {
    const content = document.querySelector("#guide-content");
    const heading = [...content.querySelectorAll("h2")].find((item) => item.textContent.startsWith(section));
    if (!heading || document.activeElement !== heading) return false;
    const area = content.getBoundingClientRect();
    const box = heading.getBoundingClientRect();
    return box.top >= area.top - 1 && box.bottom <= area.bottom + 1 &&
      (Math.abs(box.top - area.top - 12) <= 2 ||
       Math.abs(content.scrollTop - (content.scrollHeight - content.clientHeight)) <= 2);
  }, section);
}

async function learningSnapshot(page) {
  return page.evaluate(() => {
    const text = (selector) => document.querySelector(selector).textContent;
    const view = (selector) => {
      const element = document.querySelector(selector);
      return { hidden: element.hidden, text: element.textContent, scrollTop: element.scrollTop };
    };
    return {
      url: location.href, entries: history.length,
      task: text("#task-breadcrumb"), state: text("#task-state"), file: text("#active-file"),
      source: document.querySelector("#source-editor").value,
      welcomeHidden: document.querySelector("#welcome-panel").hidden,
      lesson: view("#lesson-content"), hint: view("#hint-content"),
      hintHidden: document.querySelector("#hint-panel").hidden,
      hintLevel: document.querySelector('[data-hint][aria-pressed="true"]')?.dataset.hint || null,
      acceptance: view("#history-panel"), saveState: text("#save-state"),
      dirtyFiles: [...document.querySelectorAll("#file-list .file-dirty-dot")].map((item) => item.textContent)
    };
  });
}

async function pendingEditorFallback(environment, manual) {
  const { page, baseUrl, pendingEditorRequests, releaseEditorRequests } = environment;
  await select(page, "w1.hello-world");
  await Promise.all([page.waitForRequest(EDITOR_CDN), page.locator("#start-button").click()]);
  assert.equal(await page.locator("#editor-loading").isVisible(), true);
  assert.equal(await page.locator("#editor-fallback-button").textContent(), "立即使用基础编辑器");
  assert.ok(pendingEditorRequests() > 0, "the CDN request must remain pending, not fail immediately");
  if (manual) await page.locator("#editor-fallback-button").click();
  await page.waitForFunction(() =>
    document.querySelector("#editor-engine-status").textContent === "基础编辑器（离线）" &&
    !document.querySelector("#source-editor").disabled && !document.querySelector("#source-editor").readOnly,
  null, { timeout: manual ? 5000 : 12000 });
  assert.ok(pendingEditorRequests() > 0, "fallback must settle before the held CDN response is released");
  assert.equal(await page.locator("#editor-loading").isVisible(), false);
  await releaseEditorRequests();
  await saveSource(page, HELLO);
  const saved = await (await page.request.get(baseUrl + "/api/v1/workspaces/w1.hello-world/files/src/main.cpp")).json();
  assert.equal(saved.content, HELLO, "the recovered editor must save to the real workspace");
  await page.locator("#check-button").click();
  await page.waitForFunction(() => document.querySelector("#check-result").textContent === "PASS", null, { timeout: 30000 });
  const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w1.hello-world/history")).json();
  assert.equal(history.count, 1, "fallback must remain usable through a real formal acceptance");
}

const cases = {
  async guide_navigation({ page, baseUrl, workspace }) {
    const catalog = await (await page.request.get(baseUrl + "/api/v1/tasks")).json();
    const original = await learningSnapshot(page);
    const requests = [];
    const observe = (request) => requests.push([request.method(), new URL(request.url()).pathname]);
    page.on("request", observe);
    try {
      await page.locator("#welcome-guide").click();
      await guideReady(page);
      assert.equal(await page.locator("#learning-guide").evaluate((element) => element.matches(":modal")), true);
      assert.equal(await page.locator("#guide-close").evaluate((element) => document.activeElement === element), true);
      const ids = await page.locator("#guide-content .guide-task-button").evaluateAll((buttons) =>
        buttons.map((button) => button.dataset.taskId));
      assert.equal(ids.length, 23);
      assert.deepEqual(ids.sort(), catalog.tasks.map((task) => task.id).sort(),
        "the real Markdown must expose each registered course exactly once");
      const sections = ["第一次使用", "常用词解释", "按阶段学习", "常见错误"];
      assert.deepEqual(await page.locator("#guide-sections button").evaluateAll((buttons) =>
        buttons.map((button) => button.dataset.guideSection)), sections);

      fs.mkdirSync(ARTIFACTS, { recursive: true });
      for (const width of [1440, 390]) {
        await page.setViewportSize({ width, height: width === 390 ? 844 : 1000 });
        const background = await page.evaluate(() => [scrollX, scrollY]);
        for (const section of sections) {
          await guideSection(page, section);
          assert.deepEqual(await page.evaluate(() => [scrollX, scrollY]), background,
            "keyboard section navigation must scroll only the guide");
          assert.equal(page.url(), original.url, "guide sections must not replace the learning URL");
        }
        await page.locator("#guide-close").focus();
        await page.keyboard.press("Shift+Tab");
        const boundaryFocus = await page.evaluate(() => {
          const active = document.activeElement;
          return {
            inGuide: document.querySelector("#learning-guide").contains(active),
            documentFocused: document.hasFocus(),
            documentRoot: (active === document.body || active === document.documentElement) &&
              !active.hasAttribute("tabindex") && !active.isContentEditable,
            activeTag: active?.tagName, activeId: active?.id
          };
        });
        // Native modal dialogs keep the page inert but allow browser toolbar
        // navigation. An unconfigured BODY/HTML active-element fallback is not
        // a background control. Playwright may emulate document.hasFocus(), so
        // keep that value diagnostic and verify the actual modal boundary below.
        console.log("Guide keyboard boundary (" + width + "px): " + JSON.stringify(boundaryFocus));
        assert.ok(boundaryFocus.inGuide || boundaryFocus.documentRoot,
          "backward navigation must not focus background content: " + JSON.stringify(boundaryFocus));
        await page.keyboard.press("Tab");
        assert.equal(await page.evaluate(() => document.querySelector("#learning-guide").contains(document.activeElement)), true,
          "forward navigation must return to the guide from the document boundary");
        await page.locator("#task-search").evaluate((element) => element.focus());
        assert.equal(await page.evaluate(() => document.querySelector("#learning-guide").contains(document.activeElement)), true,
          "the background course search must remain inert while the guide is modal");

        const tables = page.locator("#guide-content .markdown-table-scroll");
        assert.equal(await tables.count(), 2);
        for (const table of await tables.all()) {
          await table.focus();
          const scrolling = await table.evaluate((element) => {
            element.scrollLeft = element.scrollWidth;
            const last = element.querySelector("th:last-child").getBoundingClientRect();
            const box = element.getBoundingClientRect();
            return { overflow: getComputedStyle(element).overflowX, left: element.scrollLeft,
              maximum: element.scrollWidth - element.clientWidth, lastRight: last.right,
              visibleRight: box.left + element.clientLeft + element.clientWidth };
          });
          assert.ok(["auto", "scroll"].includes(scrolling.overflow));
          if (width === 390) assert.ok(scrolling.maximum > 0, "guide tables must scroll inside the narrow dialog");
          assert.ok(Math.abs(scrolling.left - scrolling.maximum) <= 2 && scrolling.lastRight <= scrolling.visibleRight + 2,
            "each guide table's final column must be reachable");
        }
        const content = await page.locator("#guide-content").evaluate((element) => {
          element.scrollTo({ top: element.scrollHeight, behavior: "instant" });
          const box = element.getBoundingClientRect();
          const last = element.lastElementChild.getBoundingClientRect();
          const dialog = element.closest("dialog").getBoundingClientRect();
          const close = document.querySelector("#guide-close").getBoundingClientRect();
          return { overflow: getComputedStyle(element).overflowY, top: element.scrollTop,
            maximum: element.scrollHeight - element.clientHeight, lastBottom: last.bottom, bottom: box.bottom,
            horizontalOverflow: element.scrollWidth - element.clientWidth,
            fits: dialog.left >= 0 && dialog.right <= innerWidth && dialog.top >= 0 && dialog.bottom <= innerHeight,
            closeVisible: close.top >= dialog.top && close.bottom <= dialog.bottom && close.right <= dialog.right };
        });
        assert.ok(["auto", "scroll"].includes(content.overflow));
        assert.ok(content.maximum > 0 && Math.abs(content.top - content.maximum) <= 2 && content.lastBottom <= content.bottom + 2,
          "the final guide paragraph must be reachable within its own vertical scroller");
        assert.ok(content.fits && content.closeVisible && content.horizontalOverflow <= 1,
          "the dialog, close control and content must fit at " + width + "px");
        assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
        await page.locator("#guide-content").evaluate((element) => {
          element.querySelectorAll(".markdown-table-scroll").forEach((table) => { table.scrollLeft = 0; });
        });
        await guideSection(page, "第一次使用");
        await page.locator("#guide-close").focus();
        await page.screenshot({ path: path.join(ARTIFACTS, "learning-guide-" + width + ".png") });
      }
      await page.keyboard.press("Escape");
      await page.waitForFunction(() => !document.querySelector("#learning-guide").open && document.activeElement.id === "welcome-guide");
      assert.deepEqual(await learningSnapshot(page), original, "reading and closing the welcome guide must preserve learning state");
      await page.locator("#guide-button").click();
      await guideReady(page);
      await guideSection(page, "常见错误");
      await page.locator("#guide-close").click();
      assert.equal(await page.locator("#guide-button").evaluate((element) => document.activeElement === element), true);
      await page.locator("#welcome-guide").click();
      await guideReady(page);
      await page.waitForFunction(() => {
        const content = document.querySelector("#guide-content");
        const heading = [...content.querySelectorAll("h2")].find((item) => item.textContent.startsWith("第一次使用"));
        return Math.abs(heading.getBoundingClientRect().top - content.getBoundingClientRect().top - 12) <= 2;
      });
      assert.equal(await page.locator("#guide-close").evaluate((element) => document.activeElement === element), true,
        "the first-use entry scrolls to its promised steps while keeping dialog focus usable");
      await page.locator("#guide-close").click();
      await page.locator("#guide-button").click();
      await guideReady(page);
      await page.locator('#guide-content [data-task-id="w2.todo-cli"]').click();
      await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w2.todo-cli" &&
        document.querySelector("#task-state").dataset.state === "LOCKED" &&
        !document.querySelector('#course-list [data-task-id="w2.todo-cli"]').disabled &&
        !document.querySelector("#learning-guide").open && document.activeElement.id === "task-title");
      assert.match(await page.locator("#lesson-content").textContent(), /Start here/);
      assert.equal(await page.locator("#start-button").isDisabled(), true);
      assert.equal(await page.locator("#source-editor").isDisabled(), true);
      assert.deepEqual(requests.filter(([, url]) => url === "/api/v1/learning-guide"), [["GET", "/api/v1/learning-guide"]],
        "reopening the guide must reuse its page cache");
      assert.equal(requests.some(([method, url]) => method !== "GET" || url.startsWith("/api/v1/workspaces/")), false,
        "guide reading and locked-course selection must not start a workspace or run an operation");
      assert.deepEqual(fs.readdirSync(workspace), []);
      assert.equal((await (await page.request.get(baseUrl + "/api/v1/tasks/w2.todo-cli/history")).json()).count, 0);

      // A bookmarked course can finish initial navigation after the welcome
      // guide has opened. Its close action still needs a visible focus target.
      let releaseCatalog;
      let catalogRequested;
      const catalogGate = new Promise((resolve) => { releaseCatalog = resolve; });
      const catalogSeen = new Promise((resolve) => { catalogRequested = resolve; });
      const holdCatalog = async (route) => {
        catalogRequested();
        await catalogGate;
        await route.continue();
      };
      await page.route("**/api/v1/tasks", holdCatalog, { times: 1 });
      try {
        await page.reload({ waitUntil: "domcontentloaded" });
        await waitWithin(catalogSeen, 10000, "Initial guide-case catalog request did not arrive");
        await page.locator("#welcome-guide").click();
        await page.waitForFunction(() => document.querySelector("#learning-guide").open &&
          !document.querySelector("#guide-content").hidden &&
          document.querySelector("#guide-content").getAttribute("aria-busy") === "false");
        assert.equal(await page.locator("#guide-content .guide-task-button").count(), 0,
          "guide reading must be available before the catalog is ready");
        releaseCatalog();
        await guideReady(page);
        await page.waitForFunction(() => document.querySelector("#welcome-panel").hidden &&
          document.querySelector("#task-breadcrumb").textContent === "w2.todo-cli" &&
          !document.querySelector('#course-list [data-task-id="w2.todo-cli"]').disabled);
        await page.keyboard.press("Escape");
        await page.waitForFunction(() => !document.querySelector("#learning-guide").open && document.activeElement.id === "guide-button");
      } finally {
        releaseCatalog();
        await page.unroute("**/api/v1/tasks", holdCatalog);
      }
    } finally { page.off("request", observe); }
  },

  async guide_preservation({ page, baseUrl }) {
    await passHello(page);
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    await page.locator('[data-hint="2"]').click();
    await page.waitForFunction(() => document.querySelector("#hint-content").textContent.includes("std::cout"));
    await page.locator("#history-button").click();
    await page.waitForFunction(() => document.querySelectorAll("#history-list .history-record").length === 1);
    const historyUrl = baseUrl + "/api/v1/tasks/w1.hello-world/history";
    const acceptedHistory = await (await page.request.get(historyUrl)).json();
    const source = HELLO + "// keep this unsaved learning-guide draft\n";
    const sourceRoute = "**/api/v1/workspaces/w1.hello-world/files/src/main.cpp";
    const rejectSave = (route) => route.request().method() === "PUT"
      ? route.fulfill({ status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary guide save failure" }) })
      : route.continue();
    await page.route(sourceRoute, rejectSave);
    await page.route("**/api/v1/learning-guide", (route) => route.fulfill({
      status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary guide read failure" })
    }), { times: 1 });
    const requests = [];
    const observe = (request) => requests.push([request.method(), new URL(request.url()).pathname]);
    page.on("request", observe);
    try {
      // Reject autosave before opening the guide so the draft stays dirty for
      // the entire user journey, including the explicit navigation save.
      await page.locator("#source-editor").fill(source);
      await page.waitForFunction(() => document.querySelector("#save-state").textContent === "保存失败");
      const original = await learningSnapshot(page);
      assert.ok(original.dirtyFiles.some((marker) => marker.includes("●")));
      await page.locator("#guide-button").click();
      await page.waitForFunction(() => !document.querySelector("#guide-retry").hidden &&
        document.querySelector("#guide-status").textContent.includes("temporary guide read failure"));
      assert.deepEqual(await learningSnapshot(page), original, "failed guide reads must leave the lesson and draft intact");
      await page.locator("#guide-retry").click();
      await guideReady(page);
      assert.equal(await page.locator("#guide-retry").isVisible(), false);
      await guideSection(page, "常用词解释");
      assert.deepEqual(await learningSnapshot(page), original, "section navigation must preserve the active task/file route and panels");
      await page.keyboard.press("Escape");
      await page.waitForFunction(() => !document.querySelector("#learning-guide").open && document.activeElement.id === "guide-button");
      assert.deepEqual(await learningSnapshot(page), original, "guide Escape must preserve the open hint and acceptance history");
      await page.locator("#guide-button").click();
      await guideReady(page);
      await page.locator('#guide-content [data-task-id="w1.calculator"]').click();
      await page.waitForFunction(() => document.querySelector("#guide-status").textContent.includes("未能保存"));
      await position(page, "w1.hello-world", "src/main.cpp", source);
      assert.equal(await page.locator("#learning-guide").evaluate((element) => element.open), true);
      assert.equal(await page.locator("#guide-content").isVisible(), true);
      assert.deepEqual(await learningSnapshot(page), original, "a rejected navigation save must retain all learning context");
      assert.equal(await page.locator("#save-button").isDisabled(), false);
      assert.equal(requests.some(([, url]) => url === "/api/v1/tasks/w1.calculator"), false,
        "the destination must not be read after saving the current draft failed");
      assert.deepEqual(requests.filter(([, url]) => url === "/api/v1/learning-guide"),
        [["GET", "/api/v1/learning-guide"], ["GET", "/api/v1/learning-guide"]], "retry is a fresh read and later opens use the cache");
      const saved = await (await page.request.get(baseUrl + "/api/v1/workspaces/w1.hello-world/files/src/main.cpp")).json();
      assert.equal(saved.content, HELLO);
      assert.deepEqual(await (await page.request.get(historyUrl)).json(), acceptedHistory);
    } finally {
      page.off("request", observe);
      await page.unroute(sourceRoute, rejectSave);
    }
    await page.locator('#guide-content [data-task-id="w1.calculator"]').click();
    await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w1.calculator" &&
      document.querySelector("#task-state").dataset.state === "LOCKED" &&
      !document.querySelector('#course-list [data-task-id="w1.calculator"]').disabled &&
      !document.querySelector("#learning-guide").open && document.activeElement.id === "task-title");
    const saved = await (await page.request.get(baseUrl + "/api/v1/workspaces/w1.hello-world/files/src/main.cpp")).json();
    assert.equal(saved.content, source, "successful guide navigation must save the original draft first");
    assert.equal((await (await page.request.get(historyUrl)).json()).count, 1,
      "saving and opening a course must not add an acceptance record");

    let releaseRead;
    let readRequested;
    let readFinished;
    let readStarted = false;
    const readGate = new Promise((resolve) => { releaseRead = resolve; });
    const sourceSeen = new Promise((resolve) => { readRequested = resolve; });
    const sourceFinished = new Promise((resolve) => { readFinished = resolve; });
    const holdSource = async (route) => {
      if (route.request().method() !== "GET") return route.continue();
      readStarted = true;
      readRequested();
      let forwardError;
      try {
        await readGate;
        await route.continue();
      } catch (error) {
        forwardError = error;
        await route.abort().catch(() => {});
      } finally { readFinished(forwardError); }
    };
    await page.route(sourceRoute, holdSource);
    try {
      await page.locator("#guide-button").click();
      await guideReady(page);
      await page.locator('#guide-content [data-task-id="w1.hello-world"]').click();
      await waitWithin(sourceSeen, 10000, "Guide destination source read did not arrive");
      await page.keyboard.press("Escape");
      await page.locator("#task-search").focus();
      releaseRead();
      await position(page, "w1.hello-world", "src/main.cpp", source);
      assert.equal(await page.locator("#learning-guide").evaluate((element) => element.open), false);
      assert.equal(await page.locator("#task-search").evaluate((element) => document.activeElement === element), true,
        "a late workspace read must not move focus away from the control chosen after closing help");
    } finally {
      releaseRead();
      try {
        if (readStarted) {
          const forwardError = await waitWithin(sourceFinished, 10000, "Guide destination source read did not finish");
          if (forwardError) throw forwardError;
        }
      } finally { await page.unroute(sourceRoute, holdSource); }
    }
  },

  async guide_busy({ page, baseUrl }) {
    await startHello(page);
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    const buildRoute = "**/api/v1/workspaces/w1.hello-world/build";
    let release;
    let received;
    let finished;
    let routeStarted = false;
    const gate = new Promise((resolve) => { release = resolve; });
    const responseReady = new Promise((resolve) => { received = resolve; });
    const responseFinished = new Promise((resolve) => { finished = resolve; });
    const holdBuild = async (route) => {
      routeStarted = true;
      let routeError;
      try {
        const response = await route.fetch({ timeout: 30000 });
        received();
        await gate;
        await route.fulfill({ response });
      } catch (error) {
        routeError = error;
        received(error);
        await route.abort().catch(() => {});
      } finally { finished(routeError); }
    };
    await page.route(buildRoute, holdBuild, { times: 1 });
    try {
      await page.locator("#build-button").click();
      const fetchError = await waitWithin(responseReady, 35000, "Timed out waiting for the real guide-case build response");
      if (fetchError) throw fetchError;
      const original = await learningSnapshot(page);
      await page.locator("#guide-button").click();
      await guideReady(page);
      await page.locator('#guide-content [data-task-id="w2.todo-cli"]').click();
      await page.waitForFunction(() => document.querySelector("#guide-status").textContent.includes("等待完成"));
      assert.equal(await page.locator("#learning-guide").evaluate((element) => element.open), true);
      assert.equal(await page.locator("#guide-content").isVisible(), true);
      assert.equal(await page.locator("#check-button").isDisabled(), true);
      assert.equal(await page.locator('#course-list [data-task-id="w1.hello-world"]').isDisabled(), true);
      assert.deepEqual(await learningSnapshot(page), original, "busy guide navigation must retain the running course and its route");
    } finally {
      release();
      try {
        if (routeStarted) {
          const routeError = await waitWithin(responseFinished, 10000, "Timed out finishing the intercepted guide-case build request");
          if (routeError) throw routeError;
        }
      } finally { await page.unroute(buildRoute, holdBuild); }
    }
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "构建成功" &&
      !document.querySelector('#course-list [data-task-id="w1.hello-world"]').disabled);
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    assert.equal(await page.locator("#learning-guide").evaluate((element) => element.open), true,
      "finishing the build must not dismiss the guide");
    await page.locator('#guide-content [data-task-id="w2.todo-cli"]').click();
    await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w2.todo-cli" &&
      !document.querySelector("#learning-guide").open && document.activeElement.id === "task-title");
    assert.equal(await page.locator("#start-button").isDisabled(), true);
    assert.equal((await (await page.request.get(baseUrl + "/api/v1/tasks/w1.hello-world/history")).json()).count, 0,
      "building while reading the guide must not create acceptance evidence");
  },

  async session({ page, baseUrl }) {
    await passHello(page);
    const bookmark = baseUrl + "/#" + new URLSearchParams({ task: "w1.calculator", file: "src/calculator.cpp" });
    await page.goto(bookmark);
    await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w1.calculator" &&
      document.querySelector("#task-state").dataset.state === "AVAILABLE" &&
      !document.querySelector('#course-list [data-task-id="w1.calculator"]').disabled);
    assert.equal(page.url(), bookmark, "an unstarted course retains its pending bookmarked file");
    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w1.calculator" &&
      !document.querySelector("#start-button").disabled);
    assert.equal(page.url(), bookmark, "refresh before Start must retain the file candidate");
    await page.locator("#start-button").click();
    await position(page, "w1.calculator", "src/calculator.cpp");
    const savedSource = await page.locator("#source-editor").inputValue() + "\n// saved session position\n";
    await saveSource(page, savedSource);
    const calculatorUrl = page.url();
    await page.reload({ waitUntil: "domcontentloaded" });
    await position(page, "w1.calculator", "src/calculator.cpp", savedSource);
    assert.equal(page.url(), calculatorUrl, "reload restores the explicit task/file route without another course click");

    const entries = await page.evaluate(() => history.length);
    await select(page, "w1.hello-world");
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    assert.equal(await page.evaluate(() => history.length), entries + 1, "course selection adds a history entry");
    await page.goBack();
    await position(page, "w1.calculator", "src/calculator.cpp", savedSource);
    await page.goForward();
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    await select(page, "w1.calculator");
    await position(page, "w1.calculator", "src/calculator.cpp", savedSource);

    await page.goto(baseUrl + "/");
    await page.waitForFunction(() => !document.querySelector("#welcome-panel").hidden);
    await page.reload({ waitUntil: "domcontentloaded" });
    await page.waitForFunction(() => !document.querySelector("#resume-button").hidden &&
      !document.querySelector("#resume-button").disabled);
    assert.equal(await page.locator("#workbench-panel").isVisible(), false);
    assert.match(await page.locator("#resume-button").textContent(), /Multi-file Calculator/);
    await page.locator("#resume-button").click();
    await position(page, "w1.calculator", "src/calculator.cpp", savedSource);

    const reads = [];
    const observe = (request) => {
      const pathname = new URL(request.url()).pathname;
      if (pathname.startsWith("/api/v1/")) reads.push([request.method(), pathname]);
    };
    page.on("request", observe);
    try {
      // Query changes force genuine startup reads rather than a same-document hash transition.
      await page.goto(baseUrl + "/?bookmark=unknown#task=missing.lesson&file=src%2Fmain.cpp");
      await page.waitForFunction(() => document.querySelectorAll("#course-list .task-link").length === 23 && !location.hash);
      assert.equal(await page.locator("#welcome-panel").isVisible(), true);
      assert.equal(reads.some(([, url]) => url.includes("missing.lesson")), false,
        "an unknown bookmarked task must not reach a task API endpoint");

      const invalidFile = new URLSearchParams({ task: "w1.calculator", file: "../outside.cpp" });
      await page.goto(baseUrl + "/?bookmark=file#" + invalidFile);
      await position(page, "w1.calculator", "include/calculator.hpp");
      assert.equal(reads.some(([, url]) => url.includes("outside.cpp")), false,
        "an undeclared bookmarked file must never reach the workspace API");
      await page.goto(baseUrl + "/?bookmark=locked#task=w2.todo-cli&file=src%2Ftodo.cpp");
      await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w2.todo-cli" &&
        document.querySelector("#task-state").dataset.state === "LOCKED" &&
        !document.querySelector('#course-list [data-task-id="w2.todo-cli"]').disabled);
      assert.match(await page.locator("#lesson-content").textContent(), /Start here/);
      assert.equal(await page.locator("#start-button").isDisabled(), true);
      assert.equal(reads.some(([, url]) => url.startsWith("/api/v1/workspaces/w2.todo-cli/")), false,
        "a locked lesson bookmark must not create or open a workspace");
      assert.equal(reads.some(([method, url]) => method === "POST" && url.endsWith("/w2.todo-cli/start")), false);
    } finally { page.off("request", observe); }
    const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w1.calculator/history")).json();
    assert.equal(history.count, 0, "session restoration and bookmark recovery must not create acceptance evidence");
  },

  async navigation_recovery({ page, baseUrl }) {
    await startCalculator(page);
    const source = await page.locator("#source-editor").inputValue() + "\n// retained through navigation failure\n";
    const originalUrl = page.url();
    const lesson = await page.locator("#lesson-content").textContent();
    const sourceRoute = "**/api/v1/workspaces/w1.calculator/files/src/calculator.cpp";
    const rejectSave = (route) => route.request().method() === "PUT"
      ? route.fulfill({ status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary save failure" }) })
      : route.continue();
    await page.route(sourceRoute, rejectSave);
    try {
      await page.locator("#source-editor").fill(source);
      await page.locator('#course-list [data-task-id="w1.hello-world"]').click();
      await page.waitForFunction(() => document.querySelector("#toast").textContent.includes("当前代码未能保存"));
      await position(page, "w1.calculator", "src/calculator.cpp", source);
      assert.equal(page.url(), originalUrl, "failed saves retain the last successful route");
      assert.equal(await page.locator("#lesson-content").textContent(), lesson);
    } finally { await page.unroute(sourceRoute, rejectSave); }
    await saveSource(page, source);

    const fileRequests = [];
    const observeFile = (request) => fileRequests.push(new URL(request.url()).pathname);
    page.on("request", observeFile);
    try {
      await page.route("**/api/v1/workspaces/w1.calculator/files/src/main.cpp", (route) => route.fulfill({
        status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary source read failure" })
      }), { times: 1 });
      const target = "#" + new URLSearchParams({ task: "w1.calculator", file: "src/main.cpp" });
      await page.evaluate((hash) => { location.hash = hash; }, target);
      await page.waitForFunction(() => document.querySelector("#toast").textContent.includes("temporary source read failure"));
      await position(page, "w1.calculator", "src/calculator.cpp", source);
      assert.equal(page.url(), originalUrl, "failed same-course file links preserve the last successful URL");
      assert.equal(await page.locator("#lesson-content").textContent(), lesson);
      await page.evaluate((hash) => { location.hash = hash; }, target);
      await position(page, "w1.calculator", "src/main.cpp");
      await page.evaluate((hash) => { location.hash = hash; }, new URL(originalUrl).hash);
      await position(page, "w1.calculator", "src/calculator.cpp", source);
      assert.ok(fileRequests.includes("/api/v1/workspaces/w1.calculator/files/src/main.cpp"));
      assert.equal(fileRequests.includes("/api/v1/tasks/w1.calculator"), false,
        "same-course file links must not reload the course metadata");
      assert.equal(fileRequests.includes("/api/v1/workspaces/w1.calculator/files"), false,
        "same-course file links must not reset the workspace manifest");
    } finally { page.off("request", observeFile); }

    await page.route("**/api/v1/tasks/w1.hello-world", (route) => route.fulfill({
      status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary metadata failure" })
    }), { times: 1 });
    await page.locator('#course-list [data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() => document.querySelector("#toast").textContent.includes("temporary metadata failure"));
    await position(page, "w1.calculator", "src/calculator.cpp", source);
    assert.equal(page.url(), originalUrl, "failed destination reads retain the last successful route");
    assert.equal(await page.locator("#lesson-content").textContent(), lesson);
    await select(page, "w1.hello-world");

    await page.route("**/api/v1/workspaces/w1.calculator/files", (route) => route.fulfill({
      status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary file-list failure" })
    }), { times: 1 });
    await select(page, "w1.calculator");
    assert.equal(await page.locator("#workflow-action").getAttribute("data-action"), "workspace");
    assert.equal(await page.locator("#lesson-content").textContent(), lesson, "workspace errors preserve the textbook");
    assert.equal(await page.locator("#source-editor").isDisabled(), true);
    const retryRequests = [];
    const observe = (request) => retryRequests.push([request.method(), new URL(request.url()).pathname]);
    page.on("request", observe);
    try {
      await page.locator("#workflow-action").click();
      await position(page, "w1.calculator", "src/calculator.cpp", source);
      assert.equal(await page.locator("#lesson-content").textContent(), lesson);
      assert.ok(retryRequests.some(([method, url]) => method === "GET" && url.endsWith("/w1.calculator/files")));
      assert.equal(retryRequests.some(([method]) => method === "POST"), false, "workspace recovery only rereads saved work");
    } finally { page.off("request", observe); }

    await select(page, "w1.hello-world");
    await page.route(sourceRoute, (route) => route.fulfill({
      status: 503, contentType: "application/json", body: JSON.stringify({ error: "temporary initial source failure" })
    }), { times: 1 });
    await select(page, "w1.calculator");
    assert.equal(await page.locator("#workflow-action").getAttribute("data-action"), "workspace");
    assert.equal(await page.locator("#lesson-content").textContent(), lesson);
    assert.equal(await page.locator("#source-editor").isDisabled(), true);
    assert.equal(await page.locator("#save-button").isDisabled(), true);
    for (const selector of ["#file-list .file-tab", "#lesson-content .lesson-file-button"]) {
      const controls = page.locator(selector);
      assert.ok(await controls.count() > 0, "the successful manifest exposes file controls");
      assert.equal(await controls.evaluateAll((elements) => elements.every((element) => element.disabled)), true,
        "initial source-read failure keeps file controls disabled until explicit workspace retry");
    }
    await page.locator("#workflow-action").click();
    await position(page, "w1.calculator", "src/calculator.cpp", source);
    assert.equal(await page.locator("#source-editor").evaluate((element) => element.readOnly), false);
    const recoveredSource = source + "// saved after explicit workspace recovery\n";
    await saveSource(page, recoveredSource);
    const saved = await (await page.request.get(baseUrl + "/api/v1/workspaces/w1.calculator/files/src/calculator.cpp")).json();
    assert.equal(saved.content, recoveredSource);
  },

  async editor_timeout(environment) { await pendingEditorFallback(environment, false); },
  async editor_manual(environment) { await pendingEditorFallback(environment, true); },

  async tables({ page }) {
    const lessons = [
      { id: "w2.todo-cli", headers: ["Command", "Behavior"], rows: 4,
        first: ["todo add <title>", "Add a task and assign the next ID"], last: ["todo remove <id>", "Remove an existing task"] },
      { id: "w8.http-connection-policy", headers: ["Request version", "Connection options", "Result"], rows: 6,
        first: ["HTTP/1.1", "absent or no close option", "Keep-Alive"], last: ["Unsupported version", "any", "Close"] },
      { id: "w8.http-observability", headers: ["Metric", "Semantics"], rows: 18,
        first: ["accepted_connections", "Sockets successfully registered with epoll"], last: ["log_sink_failures", "User log callbacks that threw"] }
    ];
    fs.mkdirSync(ARTIFACTS, { recursive: true });
    for (const width of [1440, 390]) {
      await page.setViewportSize({ width, height: width === 390 ? 844 : 1000 });
      for (const lesson of lessons) {
        await select(page, lesson.id);
        const region = page.locator("#lesson-content .markdown-table-scroll");
        assert.equal(await region.count(), 1, lesson.id + " must render its released table");
        assert.equal(await region.getAttribute("role"), "region");
        assert.equal(await region.getAttribute("tabindex"), "0");
        assert.deepEqual(await region.locator("table thead th").allTextContents(), lesson.headers);
        const rows = region.locator("table tbody tr");
        assert.equal(await rows.count(), lesson.rows);
        assert.deepEqual(await rows.first().locator("td").allTextContents(), lesson.first);
        assert.deepEqual(await rows.last().locator("td").allTextContents(), lesson.last);
        await region.focus();
        assert.equal(await region.evaluate((element) => document.activeElement === element), true);
        const scrolling = await region.evaluate((element) => {
          element.scrollLeft = element.scrollWidth;
          const last = element.querySelector("th:last-child").getBoundingClientRect();
          const box = element.getBoundingClientRect();
          return { overflow: getComputedStyle(element).overflowX, left: element.scrollLeft,
            maximum: element.scrollWidth - element.clientWidth, lastRight: last.right,
            visibleRight: box.left + element.clientLeft + element.clientWidth };
        });
        assert.ok(["auto", "scroll"].includes(scrolling.overflow));
        if (width === 390) assert.ok(scrolling.maximum > 0, "narrow tables must scroll inside their region");
        assert.ok(Math.abs(scrolling.left - scrolling.maximum) <= 2 && scrolling.lastRight <= scrolling.visibleRight + 2,
          lesson.id + " final column must be reachable within its own scroller");
        assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1),
          lesson.id + " must not overflow the page at " + width + "px");
        if (lesson.id === "w2.todo-cli") {
          await region.evaluate((element) => {
            element.scrollLeft = 0;
            const lesson = element.closest("#lesson-content");
            lesson.scrollTop += element.getBoundingClientRect().top - lesson.getBoundingClientRect().top - 8;
          });
          await page.evaluate(() => window.scrollTo(0, 0));
          await page.waitForFunction(() => window.scrollY === 0);
          await page.screenshot({ path: path.join(ARTIFACTS, "learning-table-" + width + ".png"), fullPage: true });
        }
      }
    }
  },

  async help({ page }) {
    await select(page, "w8.http-server");
    fs.mkdirSync(ARTIFACTS, { recursive: true });
    for (const width of [1440, 390]) {
      await page.setViewportSize({ width, height: width === 390 ? 844 : 1000 });
      await page.locator('[data-hint="3"]').click();
      await page.waitForFunction(() => document.querySelector("#hint-content").textContent.includes("shutdown"));
      const scrolling = await page.locator("#hint-content").evaluate((element) => {
        element.scrollTop = element.scrollHeight;
        const box = element.getBoundingClientRect();
        const last = element.lastElementChild.getBoundingClientRect();
        return { overflow: getComputedStyle(element).overflowY, top: element.scrollTop,
          maximum: element.scrollHeight - element.clientHeight, lastBottom: last.bottom, bottom: box.bottom };
      });
      assert.ok(["auto", "scroll"].includes(scrolling.overflow), "long hints need an independent scrolling viewport");
      assert.ok(scrolling.maximum > 0 && Math.abs(scrolling.top - scrolling.maximum) <= 2,
        "the final long-hint section must be reachable by scrolling to the bottom");
      assert.ok(scrolling.lastBottom <= scrolling.bottom + 2, "the final hint paragraph must not be clipped");
      await page.screenshot({ path: path.join(ARTIFACTS, "learning-guidance-hint-3-" + width + ".png"), fullPage: true });
      await page.locator("#hint-close").click();
      assert.equal(await page.locator("#lesson-content").isVisible(), true);
      assert.equal(await page.locator("#hint-panel").isVisible(), false);
      await page.locator('[data-lesson-section="Troubleshooting"]').click();
      await page.waitForFunction(() => {
        const area = document.querySelector("#lesson-content").getBoundingClientRect();
        const heading = [...document.querySelectorAll("#lesson-content h2")].find((item) => item.textContent === "Troubleshooting").getBoundingClientRect();
        return heading.top >= area.top - 1 && heading.bottom <= area.bottom;
      });
      assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), "guidance must not overflow the mobile viewport");
      await page.screenshot({ path: path.join(ARTIFACTS, "learning-guidance-" + width + ".png"), fullPage: true });
    }
  },

  async continuation({ page, baseUrl }) {
    await startHello(page);
    assert.match(await page.locator("#workflow-help").textContent(), /构建/);
    await page.locator("#build-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "构建成功");
    assert.equal(await page.locator("#task-state").textContent(), "进行中");
    assert.equal(await page.locator("#workflow-action").getAttribute("data-task-id"), null,
      "build success must not advertise completion or unlock a next course");
    await page.locator("#check-button").click();
    await page.waitForFunction(() => document.querySelector("#workflow-action")?.dataset.taskId === "w1.calculator");
    await page.locator("#workflow-action").click();
    await page.waitForFunction(() => document.querySelector("#task-breadcrumb").textContent === "w1.calculator");
    const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w1.hello-world/history")).json();
    assert.equal(history.count, 1, "continuation must not create extra acceptance records");
  },

  async busy({ page, baseUrl }) {
    await startCalculator(page);
    const previousUrl = page.url();
    const calculatorSource = await page.locator("#source-editor").inputValue();
    await select(page, "w1.hello-world");
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    const activeUrl = page.url();
    const checkRoute = "**/api/v1/workspaces/w1.hello-world/check";
    let release;
    let received;
    let finished;
    let routeStarted = false;
    const gate = new Promise((resolve) => { release = resolve; });
    const responseReady = new Promise((resolve) => { received = resolve; });
    const responseFinished = new Promise((resolve) => { finished = resolve; });
    const holdCheck = async (route) => {
      routeStarted = true;
      let routeError;
      try {
        const response = await route.fetch({ timeout: 30000 });
        received();
        await gate;
        await route.fulfill({ response });
      } catch (error) {
        routeError = error;
        received(error);
        await route.abort().catch(() => {});
      } finally {
        finished(routeError);
      }
    };
    await page.route(checkRoute, holdCheck, { times: 1 });
    try {
      await page.locator("#check-button").click();
      const fetchError = await waitWithin(responseReady, 35000, "Timed out waiting for the real acceptance response");
      if (fetchError) throw fetchError;
      const locked = page.locator('#course-list [data-task-id="w2.todo-cli"]');
      await locked.dispatchEvent("click");
      await sleep(150);
      assert.equal(await page.locator("#task-breadcrumb").textContent(), "w1.hello-world",
        "a pending check must not switch its result onto another course");
      assert.equal(await locked.isDisabled(), true, "course navigation must communicate that the operation is busy");
      await page.goBack({ timeout: 5000 });
      await page.waitForFunction((url) => location.href === url, activeUrl);
      assert.equal(await page.locator("#source-editor").inputValue(), HELLO,
        "Back during acceptance must retain the current source and restore its route");
    } finally {
      release();
      try {
        if (routeStarted) {
          const routeError = await waitWithin(responseFinished, 10000, "Timed out finishing the intercepted acceptance request");
          if (routeError) throw routeError;
        }
      } finally {
        await page.unroute(checkRoute, holdCheck);
      }
    }
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "PASS");
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    await page.goBack({ timeout: 5000 });
    await position(page, "w1.calculator", "src/calculator.cpp", calculatorSource);
    assert.equal(page.url(), previousUrl, "a rejected busy Back must not overwrite the previous course entry");
    await page.goForward({ timeout: 5000 });
    await position(page, "w1.hello-world", "src/main.cpp", HELLO);
    assert.equal(page.url(), activeUrl, "Forward must still return to the original course after recovery");
    assert.equal(await page.locator('#course-list [data-task-id="w2.todo-cli"]').getAttribute("data-state"), "LOCKED");
    const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w2.todo-cli/history")).json();
    assert.equal(history.count, 0);
  },

  async stale({ page, baseUrl }) {
    await passHello(page);
    await page.locator("#history-button").click();
    await page.waitForFunction(() => document.querySelectorAll("#history-list .history-record").length === 1);
    const historyUrl = baseUrl + "/api/v1/tasks/w1.hello-world/history";
    const acceptedHistory = await (await page.request.get(historyUrl)).json();
    assert.equal(acceptedHistory.current_state, "PASSED");
    await page.locator("#source-editor").fill(HELLO + "// learner revision\n");
    await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
    await page.waitForFunction(() =>
      document.querySelector('#course-list [data-task-id="w1.calculator"]').dataset.state === "LOCKED");
    assert.equal(await page.locator("#task-state").textContent(), "结果已过期");
    await page.waitForFunction(() => document.querySelector("#history-summary").textContent.includes("结果已过期"));
    assert.equal(await page.locator("#history-list .history-pass").count(), 1,
      "historical PASS remains recorded even when current code is stale");
    assert.equal(await page.locator("#workflow-action").getAttribute("data-task-id"), null);

    await page.locator("#source-editor").fill(HELLO);
    await page.locator("#save-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#save-state").textContent === "已保存到本地工作区" &&
      document.querySelector("#task-state").dataset.state === "PASSED" &&
      document.querySelector('#course-list [data-task-id="w1.calculator"]').dataset.state === "AVAILABLE" &&
      document.querySelector("#history-summary").textContent.includes("已通过") &&
      document.querySelector("#workflow-action").dataset.taskId === "w1.calculator");
    assert.equal(await page.locator("#history-list .history-record").count(), 1);
    const restoredHistory = await (await page.request.get(historyUrl)).json();
    assert.deepEqual(restoredHistory, acceptedHistory, "restoring the accepted bytes must reuse the original PASS");
    const catalog = await (await page.request.get(baseUrl + "/api/v1/tasks")).json();
    assert.equal(catalog.tasks.find((task) => task.id === "w1.calculator").state, "AVAILABLE");
  },

  async recovery({ page, baseUrl }) {
    await startHello(page);
    await page.locator("#run-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "执行失败" &&
      document.querySelector("#workflow-action").dataset.action === "build");
    assert.match(await page.locator("#console-output").textContent(), /cppapplab build <workspace-directory>/);
    assert.match(await page.locator("#workflow-help").textContent(), /请先构建/);

    const editor = page.locator("#source-editor");
    const historyUrl = baseUrl + "/api/v1/tasks/w1.hello-world/history";
    // Formal acceptance can include the same compiler diagnostic from multiple
    // stages. Follow the first actionable error without assuming uniqueness.
    const diagnostic = page.locator("#diagnostics-list .diagnostic-button")
      .filter({ hasText: "CPPAPPLAB_FALLBACK_DIAGNOSTIC" }).first();
    await editor.fill("#error CPPAPPLAB_FALLBACK_DIAGNOSTIC\n" + HELLO);
    await page.locator("#workflow-action").click();
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "构建失败");
    assert.match(await page.locator("#console-output").textContent(), /src\/main\.cpp:1:\d+: error:.*CPPAPPLAB_FALLBACK_DIAGNOSTIC/);
    assert.match(await diagnostic.textContent(), /错误.*src\/main\.cpp:1:/);
    assert.equal((await (await page.request.get(historyUrl)).json()).count, 0);
    await diagnostic.click();
    assert.equal(await editor.evaluate((element) => document.activeElement === element), true);

    await page.locator("#check-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result").textContent === "未通过" &&
      document.querySelector("#task-state").dataset.state === "NEEDS WORK");
    assert.match(await page.locator("#console-output").textContent(), /src\/main\.cpp:1:\d+: error:.*CPPAPPLAB_FALLBACK_DIAGNOSTIC/);
    assert.equal(await diagnostic.isVisible(), true);
    const failedHistory = await (await page.request.get(historyUrl)).json();
    assert.equal(failedHistory.count, 1);
    assert.equal(failedHistory.records[0].result, "FAIL");
  },

  async reference({ page, baseUrl }) {
    await select(page, "w3.task-manager");
    await page.locator("#start-button").click();
    await page.waitForFunction(() => !document.querySelector("#source-editor").disabled);
    const reference = page.locator('#file-list button[title="include/task.hpp"]');
    await reference.click();
    await page.waitForFunction(() => document.querySelector("#active-file").textContent.includes("include/task.hpp"));
    const editor = page.locator("#source-editor");
    assert.equal(await editor.evaluate((element) => element.readOnly), true, "template interfaces must be copyable but read-only");
    const before = await editor.inputValue();
    await editor.focus();
    await page.keyboard.press("Control+A");
    await page.keyboard.insertText("must not overwrite the interface");
    await page.keyboard.press("Tab");
    assert.equal(await editor.inputValue(), before, "typing and fallback Tab must not modify a reference");
    assert.match(await page.locator("#save-state").textContent(), /只读/);
    await page.locator('#lesson-content [data-file-path="src/task.cpp"]').click();
    await page.waitForFunction(() => document.querySelector("#active-file").textContent === "src/task.cpp");
    assert.equal(await editor.evaluate((element) => element.readOnly), false);
    const source = await editor.inputValue();
    await editor.fill(source + "\n// editable source still saves\n");
    await page.locator("#save-button").click();
    await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
    const saved = await (await page.request.get(baseUrl + "/api/v1/workspaces/w3.task-manager/files/src/task.cpp")).json();
    assert.match(saved.content, /editable source still saves/);
    await page.locator('#lesson-content [data-file-path="include/task.hpp"]').click();
    await page.waitForFunction(() => document.querySelector("#source-editor").readOnly);
    const history = await (await page.request.get(baseUrl + "/api/v1/tasks/w3.task-manager/history")).json();
    assert.equal(history.count, 0, "reference viewing must not accept a course");
  }
};

async function main() {
  assert.ok(BINARY && fs.existsSync(BINARY), "CPPAPPLAB_TEST_BINARY must identify the real CLI");
  const browser = await chromium.launch({ headless: true,
    ...(process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH ? { executablePath: process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH } : {}) });
  const requested = process.argv.slice(2);
  let failures = 0;
  try {
    for (const name of requested.length ? requested : Object.keys(cases)) {
      assert.ok(cases[name], "unknown case: " + name);
      const environment = await fixture(browser, {
        referenceFixture: name === "reference", pendingEditor: ["editor_timeout", "editor_manual"].includes(name)
      });
      try {
        await cases[name](environment);
        console.log("PASS: learning guidance / " + name);
      } catch (error) {
        failures += 1;
        console.error("FAIL: learning guidance / " + name + "\n" + error.stack);
      } finally { await environment.close(); }
    }
  } finally { await browser.close(); }
  if (failures) process.exitCode = 1;
}

main().catch((error) => { console.error(error.stack); process.exitCode = 1; });
