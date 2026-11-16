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

async function setMonacoModel(page, pathSuffix, value) {
  await page.evaluate(({ suffix, content }) => {
    if (!window.monaco) throw new Error("Monaco is not available.");
    const model = window.monaco.editor.getModels().find((item) => item.uri.path.endsWith(suffix));
    if (!model) throw new Error("Monaco model not found: " + suffix);
    model.setValue(content);
  }, { suffix: pathSuffix, content: value });
}

async function readWorkspaceFile(page, baseUrl, taskId, relativePath) {
  const encoded = relativePath.split("/").map(encodeURIComponent).join("/");
  const response = await page.request.get(
    baseUrl + "/api/v1/workspaces/" + encodeURIComponent(taskId) + "/files/" + encoded
  );
  assert.equal(response.status(), 200, "workspace source GET must succeed");
  return response.json();
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
    await page.waitForFunction(() => document.querySelector("#task-count")?.textContent.trim() === "23");

    const helloTask = page.locator("#course-list .task-link").filter({ hasText: "First C++ Program" });
    assert.equal(await helloTask.count(), 1, "W1 Hello World task must appear in the course list");
    await helloTask.click();
    await page.waitForFunction(() =>
      document.querySelector("#task-title")?.textContent.includes("First C++ Program")
    );
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪"
    );
    await page.waitForFunction(() => {
      const value = document.querySelector("#editor-engine-status")?.textContent || "";
      return value.startsWith("Monaco ") || value === "基础编辑器（离线）";
    }, null, { timeout: 45000 });

    const editorMode = await page.locator("#editor-engine-status").textContent();
    assert.match(editorMode, /^Monaco /, "Primary browser acceptance requires the pinned Monaco editor to load");

    await setMonacoModel(page, "/src/main.cpp", HELLO_SOURCE);
    await page.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.hello-world/files/src/main.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("Hello, CppAppLab!");
    }, null, { timeout: 10000 });
    const helloSaved = await readWorkspaceFile(page, service.baseUrl, "w1.hello-world", "src/main.cpp");
    assert.equal(helloSaved.content, HELLO_SOURCE, "Monaco autosave must persist the current source");

    await page.locator("#build-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 45000 });
    assert.match(await page.locator("#console-output").textContent(), /Build result: PASS/);

    await page.locator("#run-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "执行成功", null, { timeout: 15000 });
    assert.match(await page.locator("#console-output").textContent(), /Hello, CppAppLab!/);
    assert.equal(await page.locator("#task-state").textContent(), "进行中",
      "Build and Run must not mark the task as formally passed");

    await page.locator("#check-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "PASS", null, { timeout: 45000 });
    await page.waitForFunction(() => {
      const button = Array.from(document.querySelectorAll("#course-list .task-link"))
        .find((item) => item.textContent.includes("Multi-file Calculator"));
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    const calculatorTask = page.locator("#course-list .task-link").filter({ hasText: "Multi-file Calculator" });
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

    await fileTabs.filter({ hasText: "src/main.cpp" }).click();
    assert.equal((await page.locator("#active-file").textContent()).trim(), "src/main.cpp");
    await fileTabs.filter({ hasText: "include/calculator.hpp" }).click();
    assert.equal((await page.locator("#active-file").textContent()).trim(), "include/calculator.hpp");
    await fileTabs.filter({ hasText: "src/calculator.cpp" }).click();
    assert.equal((await page.locator("#active-file").textContent()).trim(), "src/calculator.cpp");
    await page.waitForFunction(() => window.monaco && window.monaco.editor.getModels().length === 3);

    await setMonacoModel(page, "/src/calculator.cpp", CALCULATOR_INVALID);
    await page.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.calculator/files/src/calculator.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("missing_symbol");
    }, null, { timeout: 10000 });

    await page.locator("#build-button").click();
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

    await setMonacoModel(page, "/src/calculator.cpp", CALCULATOR_SOLUTION);
    await page.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.calculator/files/src/calculator.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("case Operation::Divide") &&
        !data.content.includes("missing_symbol");
    }, null, { timeout: 10000 });

    await page.locator("#build-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 60000 });
    assert.match(await page.locator("#console-output").textContent(), /Build result: PASS/);

    await page.locator("#run-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "执行成功", null, { timeout: 60000 });
    assert.match(await page.locator("#console-output").textContent(), /100% tests passed, 0 tests failed out of 9/);

    await page.locator("#check-button").click();
    await page.waitForFunction(() => document.querySelector("#check-result")?.textContent === "PASS", null, { timeout: 60000 });
    await page.waitForFunction(() => {
      const button = Array.from(document.querySelectorAll("#course-list .task-link"))
        .find((item) => item.textContent.includes("Persistent Todo CLI"));
      return button && button.querySelector("small")?.textContent === "可开始";
    }, null, { timeout: 10000 });

    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "cppapplab-workbench-desktop.png"),
      fullPage: true
    });

    await page.setViewportSize({ width: 390, height: 844 });
    await page.waitForTimeout(500);
    const mobileOverflow = await page.evaluate(() =>
      document.documentElement.scrollWidth > document.documentElement.clientWidth + 1
    );
    assert.equal(mobileOverflow, false, "Mobile workbench must not introduce horizontal page overflow");
    await page.screenshot({
      path: path.join(ARTIFACT_DIR, "cppapplab-workbench-mobile.png"),
      fullPage: true
    });

    assert.deepEqual(pageErrors, [], "The Monaco workbench must not emit uncaught browser exceptions");
    await context.close();

    fallbackService = await startService("offline");
    const fallbackContext = await browser.newContext({
      viewport: { width: 1280, height: 900 }
    });
    const fallbackPage = await fallbackContext.newPage();
    await fallbackPage.route("https://**", (route) => route.abort());
    const fallbackErrors = [];
    fallbackPage.on("pageerror", (error) => fallbackErrors.push(error.message));

    await fallbackPage.goto(fallbackService.baseUrl + "/", { waitUntil: "domcontentloaded" });
    await fallbackPage.waitForFunction(() =>
      document.querySelector("#engine-status")?.textContent.includes("本地引擎已连接")
    );
    const fallbackHello = fallbackPage.locator("#course-list .task-link").filter({ hasText: "First C++ Program" });
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
      { timeout: 15000 }
    );
    assert.equal(await fallbackPage.locator("#source-editor").isVisible(), true,
      "Offline fallback textarea must be visible");
    await fallbackPage.locator("#source-editor").fill(HELLO_SOURCE);
    await fallbackPage.waitForFunction(async () => {
      const response = await fetch("/api/v1/workspaces/w1.hello-world/files/src/main.cpp");
      if (!response.ok) return false;
      const data = await response.json();
      return data.content.includes("Hello, CppAppLab!");
    }, null, { timeout: 10000 });
    await fallbackPage.locator("#build-button").click();
    await fallbackPage.waitForFunction(() => document.querySelector("#check-result")?.textContent === "构建成功", null, { timeout: 45000 });
    await fallbackPage.locator("#run-button").click();
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
