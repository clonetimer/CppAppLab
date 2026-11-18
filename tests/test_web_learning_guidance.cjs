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

async function fixture(browser, referenceFixture = false) {
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
  // Exercise the supported offline editor with no external editor dependency.
  await context.route(/https?:\/\/(cdn\.jsdelivr\.net|gcore\.jsdelivr\.net|cdnjs\.cloudflare\.com)\//,
    (route) => route.abort());
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
    await context.close();
    child.kill("SIGKILL");
    fs.rmSync(temporary, { recursive: true, force: true });
    throw error;
  }
  return {
    page, root, baseUrl,
    async close() {
      await context.close();
      if (child.exitCode === null && child.signalCode === null) {
        const stopped = new Promise((resolve) => child.once("exit", resolve));
        child.kill("SIGTERM");
        await stopped;
      }
      fs.rmSync(temporary, { recursive: true, force: true });
    }
  };
}

async function select(page, taskId) {
  await page.locator('#course-list [data-task-id="' + taskId + '"]').click();
  await page.waitForFunction((id) => document.querySelector("#task-breadcrumb").textContent === id, taskId);
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
  await page.waitForFunction(() => document.querySelector("#check-result").textContent === "PASS");
}

const cases = {
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
    await startHello(page);
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
    const diagnostic = page.locator("#diagnostics-list .diagnostic-button")
      .filter({ hasText: "CPPAPPLAB_FALLBACK_DIAGNOSTIC" });
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
      const environment = await fixture(browser, name === "reference");
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
