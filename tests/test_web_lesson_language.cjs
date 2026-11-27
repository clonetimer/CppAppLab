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
const ARTIFACTS = process.env.CPPAPPLAB_BROWSER_ARTIFACT_DIR || path.join(os.tmpdir(), "cppapplab-lesson-language-artifacts");
const BASELINE = process.argv.includes("--baseline");
const CATALOG_COUNT = fs.readFileSync(path.join(ROOT, "course", "tasks.tsv"), "utf8")
  .split(/\r?\n/).filter(line => line.trim() && !line.startsWith("#")).length;
if (!BASELINE) assert.equal(CATALOG_COUNT, 24);
const LANGUAGE_KEY = "cppapplab.lesson-language.v1";
const EDITOR_CDN = /https?:\/\/(cdn\.jsdelivr\.net|gcore\.jsdelivr\.net|cdnjs\.cloudflare\.com)\//;
const HELLO = '#include <iostream>\nint main() { std::cout << "Hello, CppAppLab!\\n"; }\n';
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

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
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cppapplab-language-"));
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
    await page.waitForFunction(count => document.querySelectorAll("#course-list .task-link").length === count, CATALOG_COUNT);
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
  await selectTask(page, "w1.calculator");
  await page.locator("#start-button").click();
  await position(page, "include/calculator.hpp");
  await page.locator('#file-list button[title="src/calculator.cpp"]').click();
  await position(page, "src/calculator.cpp");
  const actions = page.locator("#editor-actions-group");
  if (await actions.evaluate((element) => element.open)) await actions.locator("summary").click();
}

async function takeEvidence(page, prefix) {
  fs.mkdirSync(ARTIFACTS, { recursive: true });
  for (const width of [1440, 390, 320]) {
    await page.setViewportSize({ width, height: width === 1440 ? 1000 : 844 });
    await page.locator("#lesson-content").evaluate((element) => { element.scrollTop = 0; });
    await page.locator(".lesson-panel").evaluate((element, narrow) => {
      window.scrollTo(0, narrow ? Math.max(0, scrollY + element.getBoundingClientRect().top - 16) : 0);
    }, width !== 1440);
    await page.screenshot({ path: path.join(ARTIFACTS, prefix + "-calculator-" + width + ".png") });
  }
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.evaluate(() => window.scrollTo(0, 0));
}

async function getJson(environment, pathname) {
  const response = await environment.page.request.get(environment.baseUrl + pathname);
  assert.equal(response.status(), 200, pathname + " must use the real successful API");
  return response.json();
}

function chineseProse(markdown, label, minimum = 8) {
  assert.equal(typeof markdown, "string", label + " must be Markdown text");
  const prose = markdown.replace(/^```[^\n]*\n[\s\S]*?^```\s*$/gm, "")
    .replace(/\\?`[^`\n]*\\?`/g, "");
  assert.ok((prose.match(/[\u3400-\u9fff]/g) || []).length >= minimum,
    label + " must contain meaningful Chinese prose, not only a translated label");
}

function hintBody(markdown, level, marker) {
  const lines = markdown.split(/\r?\n/);
  const heading = new RegExp("^## " + marker + " (\\d+)(?:\\s|$)");
  const start = lines.findIndex((line) => Number(line.match(heading)?.[1]) === level);
  assert.ok(start >= 0, "the source document declares hint " + level);
  let end = lines.findIndex((line, index) => index > start && heading.test(line));
  if (end < 0) end = lines.length;
  return lines.slice(start + 1, end).join("\n").trim();
}

async function checkCatalogTranslations(environment) {
  const catalog = await getJson(environment, "/api/v1/tasks");
  assert.equal(catalog.tasks.length, CATALOG_COUNT);
  const rows = new Map(fs.readFileSync(path.join(environment.root, "course", "tasks.tsv"), "utf8")
    .split(/\r?\n/).filter((line) => line && !line.startsWith("#"))
    .map((line) => { const fields = line.split("\t"); return [fields[0], fields]; }));
  for (let offset = 0; offset < catalog.tasks.length; offset += 4) {
    await Promise.all(catalog.tasks.slice(offset, offset + 4).map(async (item) => {
      assert.equal(item.title_translations.en, item.title, item.id + " retains its English catalog title");
      chineseProse(item.title_translations["zh-CN"], item.id + " title", 1);
      const detail = await getJson(environment, "/api/v1/tasks/" + item.id);
      const english = fs.readFileSync(path.join(environment.root, rows.get(item.id)[4]), "utf8");
      assert.equal(detail.lesson_markdown, english, item.id + " retains its complete English lesson");
      assert.equal(detail.lesson_translations.en, english);
      assert.equal(detail.lesson_translations["zh-CN"], fs.readFileSync(path.join(environment.root,
        rows.get(item.id)[4].replace(/\.md$/, ".zh-CN.md")), "utf8"));
      assert.equal(detail.title_translations.en, item.title);
      assert.equal(detail.title_translations["zh-CN"], item.title_translations["zh-CN"]);
      chineseProse(detail.lesson_translations["zh-CN"], item.id + " lesson", 80);
      for (const heading of ["从这里开始", "故障排查", "验收要求"]) {
        assert.match(detail.lesson_translations["zh-CN"], new RegExp("^## " + heading + "\\s*$", "m"));
      }
      const englishHints = fs.readFileSync(path.join(environment.root, rows.get(item.id)[5]), "utf8");
      const chineseHints = fs.readFileSync(path.join(environment.root,
        rows.get(item.id)[5].replace(/\.md$/, ".zh-CN.md")), "utf8");
      for (const level of [1, 2, 3]) {
        const hint = await getJson(environment, "/api/v1/tasks/" + item.id + "/hints/" + level);
        assert.equal(hint.level, level);
        assert.equal(hint.markdown, hint.translations.en, item.id + " retains the English hint contract");
        assert.equal(hint.markdown, hintBody(englishHints, level, "Hint"));
        assert.equal(hint.translations["zh-CN"], hintBody(chineseHints, level, "提示"));
        assert.doesNotMatch(hint.markdown, /^## Hint [123](?:\s|$)/m, "other hint levels must not leak");
        assert.doesNotMatch(hint.translations["zh-CN"], /^## 提示 [123](?:\s|$)/m);
        chineseProse(hint.translations["zh-CN"], item.id + " hint " + level);
      }
    }));
  }
  console.log("PASS: lesson language / " + CATALOG_COUNT + " real lesson maps and " + CATALOG_COUNT * 3 + " real hint maps");
  return getJson(environment, "/api/v1/tasks/w1.calculator");
}

async function learningState(page) {
  return page.evaluate(() => {
    const text = (selector) => document.querySelector(selector).textContent;
    return {
      url: location.href, historyEntries: history.length,
      task: text("#task-breadcrumb"), taskState: text("#task-state"),
      file: text("#active-file"), source: document.querySelector("#source-editor").value,
      dirty: [...document.querySelectorAll("#file-list .file-dirty-dot")].map((element) => element.textContent),
      saveState: text("#save-state"), progress: text("#progress-summary"),
      console: text("#console-output"), consoleTitle: text("#console-title"), result: text("#check-result"),
      courseStates: [...document.querySelectorAll("#course-list .task-link")]
        .map((element) => [element.dataset.taskId, element.dataset.state]),
      hintHidden: document.querySelector("#hint-panel").hidden,
      hintLevel: document.querySelector('[data-hint][aria-pressed="true"]')?.dataset.hint || null
    };
  });
}

async function withoutApiRequests(page, action, message) {
  const requests = [];
  const observe = (request) => {
    if (new URL(request.url()).pathname.startsWith("/api/")) {
      requests.push(request.method() + " " + new URL(request.url()).pathname);
    }
  };
  page.on("request", observe);
  try {
    await action();
    // Include work queued after the native change event without waiting for an
    // unrelated future autosave or depending on a network-idle heuristic.
    await page.evaluate(() => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))));
    assert.deepEqual(requests, [], message);
  } finally { page.off("request", observe); }
}

async function chooseLanguage(page, language) {
  await page.locator("#lesson-language").selectOption(language);
  await page.waitForFunction((language) => document.querySelector("#lesson-content").lang === language, language);
  assert.equal(await page.locator("#lesson-language").inputValue(), language);
}

async function hintReady(page, level, language) {
  await page.waitForFunction(({ level, language }) => {
    const hint = document.querySelector("#hint-content");
    return !document.querySelector("#hint-panel").hidden && hint.lang === language &&
      !hint.querySelector(".loading") && hint.textContent.trim().length > 10 &&
      document.querySelector('[data-hint="' + level + '"]').getAttribute("aria-pressed") === "true";
  }, { level, language });
}

async function checkDefaultAndSwitch(environment, calculator) {
  const { page } = environment;
  assert.equal(await page.locator("#lesson-content").getAttribute("lang"), "zh-CN");
  assert.equal(await page.locator("#task-title").textContent(), calculator.title_translations["zh-CN"]);
  assert.equal(await page.locator("#lesson-language-status").isVisible(), false);
  chineseProse(await page.locator("#lesson-content").textContent(), "rendered Calculator", 80);
  assert.ok((await page.locator("#lesson-content h2").allTextContents()).includes("从这里开始"));
  assert.deepEqual(await page.locator("#lesson-language option").evaluateAll((options) => options.map((option) => option.value)),
    ["zh-CN", "en"]);
  await takeEvidence(page, "after-zh-CN");
  const before = await learningState(page);
  await withoutApiRequests(page, () => chooseLanguage(page, "en"), "language switching must not request API data");
  assert.deepEqual(await learningState(page), before, "switching the textbook language preserves the working task");
  assert.equal(await page.locator("#task-title").textContent(), calculator.title);
  assert.ok((await page.locator("#lesson-content h2").allTextContents()).includes("Start here"));
  assert.equal(await page.locator("#lesson-language-status").isVisible(), false);
  await takeEvidence(page, "after-en");
  console.log("PASS: lesson language / Chinese default and in-place English switch");
}

async function checkDirtySwitch(environment) {
  const { page } = environment;
  const sourcePath = "/api/v1/workspaces/w1.calculator/files/src/calculator.cpp";
  const original = await getJson(environment, sourcePath);
  const unsaved = original.content + "\n// Learner revision retained across textbook language changes.\n";
  let release;
  let received;
  let finished;
  const gate = new Promise((resolve) => { release = resolve; });
  const requestReady = new Promise((resolve) => { received = resolve; });
  const requestFinished = new Promise((resolve) => { finished = resolve; });
  let intercepted = false;
  const pattern = "**" + sourcePath;
  const holdSave = async (route) => {
    if (route.request().method() !== "PUT") return route.continue();
    intercepted = true;
    received();
    let error;
    try { await gate; await route.continue(); }
    catch (caught) { error = caught; await route.abort().catch(() => {}); }
    finally { finished(error); }
  };
  await page.route(pattern, holdSave);
  try {
    await page.locator("#source-editor").fill(unsaved);
    await waitWithin(requestReady, 10000, "The real autosave request did not start");
    assert.equal((await getJson(environment, sourcePath)).content, original.content,
      "the test holds the actual unsaved edit before it reaches disk");
    const before = await learningState(page);
    assert.ok(before.dirty.some((value) => value.includes("●")), "the pending source must still be dirty");
    await withoutApiRequests(page, async () => {
      await chooseLanguage(page, "zh-CN");
      assert.deepEqual(await learningState(page), before);
      await chooseLanguage(page, "en");
      assert.deepEqual(await learningState(page), before);
    }, "switching an unsaved workspace must not submit or reload it");
    assert.equal(await page.locator("#source-editor").inputValue(), unsaved);
  } finally {
    release();
    if (intercepted) {
      const error = await waitWithin(requestFinished, 10000, "The held autosave did not resume");
      if (error) throw error;
    }
    await page.unroute(pattern, holdSave);
  }
  await page.waitForFunction(() => document.querySelector("#save-state").textContent === "已保存到本地工作区");
  assert.equal((await getJson(environment, sourcePath)).content, unsaved,
    "the retained edit must subsequently save through the real server");
  console.log("PASS: lesson language / unsaved code, console, progress and active file preserved");
}

async function checkHints(environment) {
  const { page } = environment;
  await chooseLanguage(page, "zh-CN");
  for (const level of [1, 2, 3]) {
    await page.locator('[data-hint="' + level + '"]').click();
    await hintReady(page, level, "zh-CN");
    chineseProse(await page.locator("#hint-content").textContent(), "visible hint " + level);
    const chineseContent = await page.locator("#hint-content").innerHTML();
    const before = await learningState(page);
    await withoutApiRequests(page, async () => {
      await chooseLanguage(page, "en");
      await hintReady(page, level, "en");
      assert.notEqual(await page.locator("#hint-content").innerHTML(), chineseContent,
        "the body changes with the language, not only the control label");
      assert.deepEqual(await learningState(page), before, "an open hint retains its task and hint level");
      await chooseLanguage(page, "zh-CN");
      await hintReady(page, level, "zh-CN");
      assert.equal(await page.locator("#hint-content").innerHTML(), chineseContent);
      assert.deepEqual(await learningState(page), before);
    }, "both languages of an open hint are already loaded");
  }
  await page.screenshot({ path: path.join(ARTIFACTS, "after-zh-CN-calculator-hint-3-1440.png") });
  await page.locator("#hint-close").click();
  console.log("PASS: lesson language / all three hint levels switch without refetching");
}

async function checkSectionsAndLinks(environment) {
  const { page } = environment;
  const sections = [
    ["Start here", "从这里开始"], ["Troubleshooting", "故障排查"], ["Acceptance", "验收要求"]
  ];
  for (const language of ["zh-CN", "en"]) {
    await chooseLanguage(page, language);
    const before = await learningState(page);
    for (const [logical, chinese] of sections) {
      const expected = language === "en" ? logical : chinese;
      await page.locator('[data-lesson-section="' + logical + '"]').click();
      await page.waitForFunction((expected) => {
        const content = document.querySelector("#lesson-content");
        const heading = [...content.querySelectorAll("h2")].find((element) => element.textContent.trim() === expected);
        if (!heading || document.activeElement !== heading) return false;
        const region = content.getBoundingClientRect();
        const box = heading.getBoundingClientRect();
        return box.top >= region.top - 1 && box.bottom <= region.bottom + 1;
      }, expected);
      assert.deepEqual(await learningState(page), before, "a translated section jump retains the learning position");
    }
    const paths = await page.locator("#lesson-content [data-file-path]").evaluateAll((elements) =>
      [...new Set(elements.map((element) => element.dataset.filePath))].sort());
    for (const file of ["src/main.cpp", "include/calculator.hpp", "src/calculator.cpp"]) {
      assert.ok(paths.includes(file), language + " retains the clickable course file " + file);
      await page.locator('#lesson-content [data-file-path="' + file + '"]').first().click();
      await position(page, file);
      const source = await getJson(environment, "/api/v1/workspaces/w1.calculator/files/" + file);
      assert.equal(await page.locator("#source-editor").inputValue(), source.content);
      assert.equal(await page.locator("#lesson-content").getAttribute("lang"), language);
    }
  }
  console.log("PASS: lesson language / translated section jumps and real source-file links");
}

async function checkSearch(environment, calculator) {
  const { page } = environment;
  for (const language of ["zh-CN", "en"]) {
    await chooseLanguage(page, language);
    for (const query of [calculator.title_translations["zh-CN"], calculator.title]) {
      await page.locator("#task-search").fill(query);
      const matches = page.locator("#course-list .task-link");
      assert.equal(await matches.count(), 1, "both title languages find Calculator while reading " + language);
      assert.equal(await matches.first().getAttribute("data-task-id"), "w1.calculator");
      const label = calculator.title_translations[language].replace(/^W\d+\s*[·:]\s*/, "");
      assert.ok((await matches.first().textContent()).includes(label));
    }
    await page.locator("#task-search").fill("");
    assert.equal(await page.locator("#course-list .task-link").count(), CATALOG_COUNT);
  }
  console.log("PASS: lesson language / Chinese and English title search");
}

async function checkDelayedHint(environment) {
  const { page } = environment;
  await chooseLanguage(page, "en");
  let release;
  let received;
  let finished;
  const gate = new Promise((resolve) => { release = resolve; });
  const responseReady = new Promise((resolve) => { received = resolve; });
  const responseFinished = new Promise((resolve) => { finished = resolve; });
  let intercepted = false;
  const pattern = "**/api/v1/tasks/w1.calculator/hints/1";
  const holdHint = async (route) => {
    intercepted = true;
    let error;
    try {
      const response = await route.fetch();
      assert.equal(response.status(), 200);
      received();
      await gate;
      await route.fulfill({ response });
    } catch (caught) {
      error = caught;
      received(error);
      await route.abort().catch(() => {});
    } finally { finished(error); }
  };
  await page.route(pattern, holdHint, { times: 1 });
  try {
    await page.locator('[data-hint="1"]').click();
    const error = await waitWithin(responseReady, 10000, "The real hint response did not arrive");
    if (error) throw error;
    assert.equal(await page.locator("#hint-panel").isVisible(), true);
    await withoutApiRequests(page, () => chooseLanguage(page, "zh-CN"),
      "changing language during a pending hint must not issue a second request");
  } finally {
    release();
    if (intercepted) {
      const error = await waitWithin(responseFinished, 10000, "The delayed hint did not resume");
      if (error) throw error;
    }
    await page.unroute(pattern, holdHint);
  }
  await hintReady(page, 1, "zh-CN");
  chineseProse(await page.locator("#hint-content").textContent(), "late hint in the current language");
  assert.equal(await page.locator('[data-hint="1"]').getAttribute("aria-pressed"), "true");
  await page.locator("#hint-close").click();
  console.log("PASS: lesson language / delayed real hint uses the latest language");
}

async function checkPersistenceAndNarrowLayout(environment, calculator) {
  const { page } = environment;
  await chooseLanguage(page, "en");
  assert.equal(await page.evaluate((key) => localStorage.getItem(key), LANGUAGE_KEY), "en");
  await page.reload();
  await position(page, "src/calculator.cpp");
  assert.equal(await page.locator("#lesson-language").inputValue(), "en");
  assert.equal(await page.locator("#lesson-content").getAttribute("lang"), "en");
  assert.equal(await page.locator("#task-title").textContent(), calculator.title);
  await chooseLanguage(page, "zh-CN");
  for (const width of [1440, 390, 320]) {
    await page.setViewportSize({ width, height: width === 1440 ? 1000 : 844 });
    const selector = page.locator("#lesson-language");
    await selector.scrollIntoViewIfNeeded();
    await selector.focus();
    const geometry = await selector.evaluate((element) => {
      const box = element.getBoundingClientRect();
      return { height: box.height, left: box.left, right: box.right, top: box.top, bottom: box.bottom,
        width: innerWidth, viewportHeight: innerHeight, focusable: element.tabIndex >= 0 && !element.disabled,
        focused: document.activeElement === element, pageWidth: document.documentElement.scrollWidth };
    });
    assert.ok(geometry.height >= 44 && geometry.focusable && geometry.focused,
      "the native language control remains a keyboard-reachable 44px target at " + width + "px");
    assert.ok(geometry.left >= 0 && geometry.right <= geometry.width + 1 && geometry.top >= 0 &&
      geometry.bottom <= geometry.viewportHeight + 1 && geometry.pageWidth <= geometry.width + 1,
    "the selector must be reachable without horizontal page overflow at " + width + "px: " + JSON.stringify(geometry));
    await chooseLanguage(page, "en");
    await chooseLanguage(page, "zh-CN");
  }
  await page.setViewportSize({ width: 1440, height: 1000 });
  console.log("PASS: lesson language / reload persistence and 1440px, 390px, 320px accessibility");
}

async function checkMissingTranslation(environment) {
  const { page, root } = environment;
  const translation = path.join(root, "course", "lessons", "w1", "calculator.zh-CN.md");
  const original = fs.readFileSync(translation);
  fs.unlinkSync(translation);
  try {
    await page.reload();
    await position(page, "src/calculator.cpp");
    assert.equal(await page.locator("#lesson-language").inputValue(), "zh-CN");
    assert.equal(await page.locator("#lesson-content").getAttribute("lang"), "en",
      "a missing translation must report the actual rendered English language");
    assert.equal(await page.locator("#lesson-language-status").isVisible(), true);
    assert.match(await page.locator("#lesson-language-status").textContent(), /中文.*英文/);
    assert.ok((await page.locator("#lesson-content h2").allTextContents()).includes("Start here"));
  } finally { fs.writeFileSync(translation, original); }
  await page.reload();
  await position(page, "src/calculator.cpp");
  assert.equal(await page.locator("#lesson-content").getAttribute("lang"), "zh-CN");
  assert.equal(await page.locator("#lesson-language-status").isVisible(), false);
  console.log("PASS: lesson language / genuine missing-file fallback is labelled and recovers");
}

async function main() {
  assert.ok(BINARY && fs.existsSync(BINARY), "CPPAPPLAB_TEST_BINARY must identify the real CLI");
  const browser = await chromium.launch({ headless: true,
    ...(process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH ? { executablePath: process.env.CPPAPPLAB_BROWSER_EXECUTABLE_PATH } : {}) });
  let environment;
  try {
    environment = await fixture(browser);
    if (!BASELINE) {
      assert.equal(await environment.page.locator("select#lesson-language").count(), 1,
        "the lesson panel must expose its native Chinese/English language selector");
      assert.equal(await environment.page.locator("#lesson-language").inputValue(), "zh-CN",
        "a fresh learner defaults to Simplified Chinese");
    }
    const calculator = BASELINE ? null : await checkCatalogTranslations(environment);
    await startCalculator(environment.page);
    const history = await getJson(environment, "/api/v1/tasks/w1.hello-world/history");
    assert.equal(history.count, 1, "Calculator is unlocked by exactly one real Hello acceptance");
    assert.equal(history.records[0].result, "PASS");
    assert.equal(history.current_state, "PASSED");
    if (BASELINE) {
      await takeEvidence(environment.page, "before");
      console.log("PASS: lesson language / untouched baseline Calculator evidence");
    } else {
      await checkDefaultAndSwitch(environment, calculator);
      await checkDirtySwitch(environment);
      await checkHints(environment);
      await checkSectionsAndLinks(environment);
      await checkSearch(environment, calculator);
      await checkDelayedHint(environment);
      await checkPersistenceAndNarrowLayout(environment, calculator);
      await checkMissingTranslation(environment);
      assert.deepEqual(environment.pageErrors, [], "language interactions must not raise browser errors");
    }
  } catch (error) {
    if (environment && !environment.page.isClosed()) {
      fs.mkdirSync(ARTIFACTS, { recursive: true });
      await environment.page.screenshot({ path: path.join(ARTIFACTS, BASELINE ? "baseline-failure.png" : "language-failure.png") })
        .catch(() => {});
    }
    throw error;
  } finally {
    if (environment) await environment.close();
    await browser.close();
  }
}

main().catch((error) => { console.error(error.stack); process.exitCode = 1; });
