"use strict";

// Native Windows DOM acceptance. No mocked API or mocked learning engine.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const net = require("node:net");
const { spawn } = require("node:child_process");
const { chromium } = require("playwright");

const ROOT = path.resolve(__dirname, "..");
const BINARY = process.env.CPPAPPLAB_TEST_BINARY;
const PYTHON = process.env.CPPAPPLAB_TEST_PYTHON || "python.exe";
const ARTIFACTS = process.env.CPPAPPLAB_BROWSER_ARTIFACT_DIR;
const HELLO = '#include <iostream>\nint main(){std::cout<<"Hello, CppAppLab!\\n";}\n';
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

async function freePort() {
  const server = net.createServer();
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const port = server.address().port;
  await new Promise((resolve, reject) => server.close(error => error ? reject(error) : resolve()));
  return port;
}

async function action(page, selector, expected) {
  const group = page.locator("#editor-actions-group");
  if (!(await group.evaluate(el => el.open))) await group.locator("summary").click();
  await page.locator(selector).click();
  await page.waitForFunction(value =>
    document.querySelector("#check-result")?.textContent.trim() === value,
    expected, { timeout: 45000 });
}

async function main() {
  assert.equal(process.platform, "win32");
  assert.ok(BINARY && fs.existsSync(BINARY), "Real native cppapplab.exe required");
  assert.ok(ARTIFACTS, "Screenshot artifact directory required");
  fs.mkdirSync(ARTIFACTS, { recursive: true });
  const base = fs.mkdtempSync(path.join(os.tmpdir(), "cppapplab-native-dom-"));
  const home = path.join(base, "学员 资料");
  fs.mkdirSync(home);
  const port = await freePort();
  const url = "http://127.0.0.1:" + port;
  const child = spawn(PYTHON, [
    path.join(ROOT, "web", "server", "cppapplab_web.py"),
    "--root", ROOT, "--cppapplab", BINARY, "--host", "127.0.0.1", "--port", String(port)
  ], {
    cwd: ROOT, windowsHide: true, stdio: ["ignore", "pipe", "pipe"],
    env: { ...process.env, USERPROFILE: home, CPPAPPLAB_STATE_DIR: path.join(base, "学习记录"),
      CPPAPPLAB_NATIVE_BUILD_ROOT: path.join(base, "cache") }
  });
  let logs = "", browser;
  let startupError;
  child.on("error", error => { startupError = error; });
  for (const stream of [child.stdout, child.stderr]) {
    stream.setEncoding("utf8");
    stream.on("data", chunk => { logs += chunk; });
  }
  try {
    let ready = false;
    for (let index = 0; index < 150; ++index) {
      if (startupError) throw startupError;
      if (child.exitCode !== null) throw new Error("Native service exited early");
      try {
        const health = await (await fetch(url + "/api/v1/health")).json();
        if (health.status === "ok" && health.execution_environment === "native-windows") {
          ready = true;
          break;
        }
      } catch {}
      await sleep(200);
    }
    assert.ok(ready, "Native service did not become ready");
    browser = await chromium.launch({ headless: true });
    const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
    const errors = [];
    page.on("pageerror", error => errors.push(error.message));
    // Force the existing offline editor branch, not a synthetic replacement.
    await page.route("**/vs/loader.js", route => route.abort());
    await page.goto(url, { waitUntil: "domcontentloaded" });
    await page.waitForFunction(() =>
      document.querySelectorAll("#course-list .task-link").length === 25 &&
      document.querySelector("#execution-environment")?.textContent.includes("Windows 原生"));
    assert.match(await page.locator("#progress-summary").innerText(), /0 \/ 5/);
    assert.equal(await page.locator('.task-link[data-state="LINUX/WSL2 REQUIRED"]').count(), 20);
    await page.locator('.task-link[data-task-id="w5.system-lab"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent === "w5.system-lab" &&
      document.querySelector("#task-state")?.textContent.includes("WSL2"));
    // F2 enables only the explicit handoff, never native Linux-course execution.
    await page.waitForFunction(() => !document.querySelector("#start-button").disabled);
    assert.match(await page.locator("#start-button").innerText(), /进入 WSL2/);
    assert.equal(await page.locator("#build-button").isDisabled(), true);
    assert.equal(await page.locator("#run-button").isDisabled(), true);
    assert.equal(await page.locator("#check-button").isDisabled(), true);
    const rejected = await page.evaluate(async () => {
      const { csrf_token } = await (await fetch("/api/v1/session")).json();
      const headers = { "Content-Type": "application/json", "X-CppAppLab-Token": csrf_token };
      const attempt = async (path, body) => (await fetch(path, {
        method: "POST", headers, body: JSON.stringify(body)
      })).status;
      return [
        await attempt("/api/v1/tasks/w5.system-lab/start", {}),
        await attempt("/api/v1/tasks/w5.system-lab/handoff", { command: "anything" }),
        await attempt("/api/v1/tasks/w1.hello-world/handoff", {})
      ];
    });
    assert.deepEqual(rejected, [409, 400, 409]);
    await page.screenshot({ path: path.join(ARTIFACTS, "windows-linux-required.png") });

    await page.locator('.task-link[data-task-id="w1.hello-world"]').click();
    await page.waitForFunction(() =>
      document.querySelector("#task-breadcrumb")?.textContent === "w1.hello-world");
    await page.locator("#start-button").click();
    await page.waitForFunction(() =>
      document.querySelector("#workspace-state")?.textContent === "工作区已就绪" &&
      document.querySelector("#editor-engine-status")?.textContent === "基础编辑器（离线）",
      null, { timeout: 20000 });
    await page.locator("#source-editor").fill(HELLO);
    await page.waitForFunction(() =>
      document.querySelector("#save-state")?.textContent === "已保存到本地工作区",
      null, { timeout: 10000 });
    await action(page, "#build-button", "构建成功");
    await action(page, "#run-button", "执行成功");
    assert.match(await page.locator("#console-output").textContent(), /Hello, CppAppLab!/);
    await action(page, "#check-button", "PASS");
    await page.waitForFunction(() =>
      document.querySelector('.task-link[data-task-id="w1.calculator"]')?.dataset.state === "AVAILABLE");
    await page.screenshot({ path: path.join(ARTIFACTS, "windows-native-w1-pass.png") });
    assert.deepEqual(errors, []);
    console.log("PASS: native Windows browser, environment distinction, offline editor, real W1 build/run/check and calculator unlock.");
  } catch (error) {
    console.error(logs);
    throw error;
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null && child.pid) {
      const exited = new Promise(resolve => child.once("exit", resolve));
      child.kill();
      await Promise.race([exited, sleep(10000)]);
    }
    fs.rmSync(base, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
  }
}
main().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
