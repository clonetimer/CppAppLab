"use strict";

// Exercise the real workbench state transitions without a browser dependency.
// Network and DOM boundaries are adapters; the functions under test are loaded
// unchanged from app.js. Layout and native editor behavior have Chromium tests.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const test = require("node:test");

class Element {
  constructor(tag = "div") {
    this.tagName = tag.toUpperCase();
    this.children = [];
    this.dataset = {};
    this.attributes = {};
    this.listeners = {};
    this.classList = { add() {}, remove() {}, toggle() {} };
    this.textContent = "";
    this.innerHTML = "";
    this.value = "";
    this.hidden = false;
    this.disabled = false;
    this.readOnly = false;
    this.selectionStart = 0;
    this.selectionEnd = 0;
    this.open = false;
    this.focusCalls = 0;
    this.scrollTop = 0;
    this.parentElement = null;
  }
  append(...items) { for (const item of items) item.parentElement = this; this.children.push(...items); }
  replaceChildren(...items) { this.children = []; this.append(...items); }
  get firstChild() { return this.children[0] || null; }
  replaceWith(...items) {
    const parent = this.parentElement;
    if (!parent) return;
    for (const item of items) item.parentElement = parent;
    parent.children.splice(parent.children.indexOf(this), 1, ...items);
    this.parentElement = null;
  }
  setAttribute(name, value) { this.attributes[name] = String(value); }
  removeAttribute(name) {
    delete this.attributes[name];
    if (name === "data-task-id") delete this.dataset.taskId;
  }
  addEventListener(name, listener) { (this.listeners[name] ||= []).push(listener); }
  fire(name, event) { for (const listener of this.listeners[name] || []) listener(event); }
  querySelectorAll(selector) {
    const all = this.children.flatMap((child) => [child, ...child.querySelectorAll("*")]);
    if (selector === "*") return all;
    if ([".task-link", ".guide-task-button"].includes(selector)) return all.filter((child) => child.className?.split(" ").includes(selector.slice(1)));
    if (selector === "li > strong:first-child") return all.filter((child) => child.tagName === "STRONG" && child.parentElement?.tagName === "LI" && child.parentElement.firstChild === child);
    if (selector === "h2") return all.filter((child) => child.tagName === "H2");
    if (selector === "[data-guide-section]") return all.filter((child) => child.dataset.guideSection);
    return [];
  }
  querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
  focus() { this.focusCalls += 1; }
  showModal() { this.open = true; }
  close() { this.open = false; }
  scrollIntoView() {}
  scrollTo(options) { this.scrollTop = options.top; }
  getBoundingClientRect() { return { top: this.top || 0 }; }
  setRangeText(text, start, end) { this.value = this.value.slice(0, start) + text + this.value.slice(end); }
}

function task(id, state, prerequisites = []) {
  return { id, state, prerequisites, title: id === "w1.calculator" ? "W1 · Calculator" : id,
    week: 1, order: 1, source_files: ["src/main.cpp"], acceptance_kind: "exact_stdout",
    runtime_profile: null, service_profile: null, state_reason: "", lesson_markdown: "# Lesson" };
}

function application(options = {}) {
  const elements = new Map();
  const byId = (id) => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
  };
  const hints = [1, 2, 3].map((level) => {
    const button = new Element("button");
    button.dataset.hint = String(level);
    return button;
  });
  const documentListeners = {};
  const document = {
    body: new Element("body"),
    getElementById: byId, createElement: (tag) => new Element(tag),
    addEventListener(name, handler) { (documentListeners[name] ||= []).push(handler); },
    querySelectorAll(selector) {
      if (selector === "[data-hint]") return hints;
      const matches = [];
      if (selector.includes(".task-link")) matches.push(...byId("course-list").querySelectorAll(".task-link"));
      if (selector.includes(".file-tab")) matches.push(...byId("file-list").children.filter((child) => child.className?.split(" ").includes("file-tab")));
      return matches;
    }
  };
  const location = new URL(options.url || "http://127.0.0.1:8765/");
  const stored = new Map(Object.entries(options.storage || {}));
  const listeners = {};
  const entries = [location.href];
  const historyStates = [null];
  const pendingHistory = [];
  let historyIndex = 0;
  const window = {
    location,
    history: {
      get state() { return historyStates[historyIndex]; },
      pushState(value, _title, url) {
        location.href = new URL(url, location).href;
        entries.splice(historyIndex + 1, entries.length, location.href);
        historyStates.splice(historyIndex + 1, historyStates.length, value);
        historyIndex += 1;
      },
      replaceState(value, _title, url) {
        location.href = new URL(url, location).href;
        entries[historyIndex] = location.href;
        historyStates[historyIndex] = value;
      },
      go(delta) {
        const next = historyIndex + delta;
        if (next < 0 || next >= entries.length || next === historyIndex) return;
        historyIndex = next;
        location.href = entries[historyIndex];
        pendingHistory.push(Promise.resolve().then(() => Promise.all((listeners.hashchange || []).map((handler) => handler()))));
      }
    },
    localStorage: {
      getItem(key) { if (options.storageDenied) throw new Error("storage denied"); return stored.get(key) || null; },
      setItem(key, value) { if (options.storageDenied) throw new Error("storage denied"); stored.set(key, value); }
    },
    addEventListener(name, handler) { (listeners[name] ||= []).push(handler); }
  };
  const context = vm.createContext({ document, console, Map, Set, URL, URLSearchParams, Headers,
    setTimeout: () => 1, clearTimeout() {}, window });
  const filename = path.join(__dirname, "..", "web", "ui", "app.js");
  const source = fs.readFileSync(filename, "utf8");
  const injected = source.replace(/\n  init\(\);\n\}\)\(\);\s*$/, `
  globalThis.workbench = { state, ui, selectTask, showHint, setEditorValue,
    handleEditorInput, saveFile, loadTasks, loadTaskHistory, runWorkspaceAction, bindEvents, setBusy,
    followWorkflowAction, refreshServiceStatus, stopNetworkService,
    init, loadFile,
    openLearningGuide: typeof openLearningGuide === "function" ? openLearningGuide : undefined,
    closeLearningGuide: typeof closeLearningGuide === "function" ? closeLearningGuide : undefined,
    loadLearningGuide: typeof loadLearningGuide === "function" ? loadLearningGuide : undefined,
    enhanceGuideCourses: typeof enhanceGuideCourses === "function" ? enhanceGuideCourses : undefined,
    jumpToGuideSection: typeof jumpToGuideSection === "function" ? jumpToGuideSection : undefined,
    selectGuideTask: typeof selectGuideTask === "function" ? selectGuideTask : undefined,
    retryWorkspaceFiles: typeof retryWorkspaceFiles === "function" ? retryWorkspaceFiles : undefined,
    setApi(handler) { api = handler; } };
})();`);
  assert.notEqual(injected, source, "test adapter must replace only automatic startup");
  vm.runInContext(injected, context, { filename });
  const app = context.workbench;
  app.ui.historyPanel.hidden = true;
  byId("hint-panel").hidden = true;
  app.state.task = task("w1.hello-world", "IN PROGRESS");
  app.state.selectedTaskId = "w1.hello-world";
  app.state.workspaceReady = true;
  app.state.editorReady = Promise.resolve(false);
  app.state.files = [{ path: "src/main.cpp", read_only: false }];
  app.state.tasks = [app.state.task, task("w1.calculator", "LOCKED", ["w1.hello-world"]),
    task("w2.todo-cli", "LOCKED", ["w1.calculator"])];
  for (const section of ["第一次使用", "常用词解释", "按阶段学习", "常见错误"]) {
    const button = new Element("button");
    button.dataset.guideSection = section;
    byId("guide-sections").append(button);
  }
  async function flushHistory() {
    while (pendingHistory.length) await Promise.all(pendingHistory.splice(0));
  }
  return { ...app, byId, location, stored, entries,
    documentEvent(name, event) { for (const handler of documentListeners[name] || []) handler(event); },
    async travel(delta) { window.history.go(delta); await flushHistory(); },
    async browserEvent(name) {
      await Promise.all((listeners[name] || []).map((handler) => handler()));
      await flushHistory();
    } };
}

test("learning guide opens and closes without changing dirty work or a running operation", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.hello-world&file=src%2Fmain.cpp" });
  app.state.activeFile = "src/main.cpp";
  app.state.busy = true;
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "// keep unsaved code";
  app.ui.lesson.hidden = true;
  app.ui.hintPanel.hidden = false;
  app.ui.historyPanel.hidden = false;
  const originalTask = app.state.task;
  const originalUrl = app.location.href;
  const requests = [];
  app.setApi(async (url, options = {}) => {
    requests.push([url, options.method || "GET"]);
    return { markdown: "# 中文学习指南\n\n<script>unsafe()</script>\n" };
  });
  app.bindEvents();
  await app.openLearningGuide(app.ui.guideButton);
  assert.equal(app.ui.guideDialog.open, true);
  assert.match(app.ui.guideContent.innerHTML, /&lt;script&gt;/);
  assert.doesNotMatch(app.ui.guideContent.innerHTML, /<script>/);
  app.documentEvent("keydown", { key: "Escape", preventDefault() {} });
  assert.equal(app.ui.historyPanel.hidden, false, "guide Escape must not close underlying history");
  app.ui.guideDialog.fire("cancel", { preventDefault() {} });
  assert.equal(app.ui.guideDialog.open, false);
  assert.equal(app.ui.guideButton.focusCalls, 1);
  assert.equal(app.state.task, originalTask);
  assert.equal(app.state.activeFile, "src/main.cpp");
  assert.equal(app.ui.editor.value, "// keep unsaved code");
  assert.equal(app.state.dirtyFiles.has("src/main.cpp"), true);
  assert.equal(app.state.busy, true);
  assert.equal(app.ui.lesson.hidden, true);
  assert.equal(app.ui.hintPanel.hidden, false);
  assert.equal(app.location.href, originalUrl);
  assert.deepEqual(requests, [["/api/v1/learning-guide", "GET"]]);
});

test("learning guide deduplicates pending reads and a late response cannot reopen or focus it", async () => {
  const app = application();
  let release;
  let reads = 0;
  app.setApi(() => { reads += 1; return new Promise((resolve) => { release = resolve; }); });
  const first = app.openLearningGuide(app.ui.welcomeGuide);
  const second = app.openLearningGuide(app.ui.guideButton);
  app.closeLearningGuide();
  const focused = app.ui.guideClose.focusCalls;
  release({ markdown: "# 缓存的指南" });
  await Promise.all([first, second]);
  assert.equal(reads, 1);
  assert.equal(app.ui.guideDialog.open, false);
  assert.equal(app.ui.guideClose.focusCalls, focused);
  assert.equal(app.ui.welcomeGuide.focusCalls, 1, "restore the original opener");
  await app.openLearningGuide(app.ui.guideButton);
  assert.equal(reads, 1, "reopening a loaded guide uses the session cache");
  assert.equal(app.ui.guideDialog.open, true);
});

test("learning guide read failure offers a GET-only retry without clearing the lesson", async () => {
  const app = application();
  app.ui.lesson.innerHTML = "Keep the active lesson";
  app.ui.editor.value = "Keep code";
  let reads = 0;
  app.setApi(async (url, options = {}) => {
    assert.equal(url, "/api/v1/learning-guide");
    assert.equal(options.method || "GET", "GET");
    if (++reads === 1) throw new Error("temporary read failure");
    return { markdown: "# 指南恢复" };
  });
  await app.openLearningGuide(app.ui.guideButton);
  assert.match(app.ui.guideStatus.textContent, /读取失败/);
  assert.equal(app.ui.guideRetry.hidden, false);
  await app.loadLearningGuide();
  assert.equal(reads, 2);
  assert.equal(app.ui.guideRetry.hidden, true);
  assert.match(app.ui.guideContent.innerHTML, /指南恢复/);
  assert.equal(app.ui.lesson.innerHTML, "Keep the active lesson");
  assert.equal(app.ui.editor.value, "Keep code");
  assert.equal(app.state.busy, false);
});

test("learning guide remains readable when engine health and catalog startup fail", async () => {
  const app = application();
  app.state.tasks = [];
  app.state.task = null;
  const requests = [];
  app.setApi(async (url) => {
    requests.push(url);
    if (url === "/api/v1/session") return { csrf_token: "session" };
    if (url === "/api/v1/health") throw new Error("engine unavailable");
    if (url === "/api/v1/learning-guide") return { markdown: "# 常见错误\n\n检查本地引擎。" };
    throw new Error("unexpected request: " + url);
  });
  await app.init();
  await app.openLearningGuide(app.ui.guideButton);
  assert.equal(app.ui.guideDialog.open, true);
  assert.match(app.ui.guideContent.innerHTML, /检查本地引擎/);
  assert.deepEqual(requests, ["/api/v1/session", "/api/v1/health", "/api/v1/learning-guide"]);
});

test("guide course controls match all registered headings and ignore prose, code and unknown IDs", () => {
  const app = application();
  const root = path.join(__dirname, "..");
  const ids = fs.readFileSync(path.join(root, "course", "tasks.tsv"), "utf8")
    .split("\n").filter((line) => line && !line.startsWith("#")).map((line) => line.split("\t")[0]);
  app.state.tasks = ids.map((id) => task(id, "LOCKED"));
  const guide = fs.readFileSync(path.join(root, "course", "LEARNING_GUIDE_ZH.md"), "utf8");
  for (const match of guide.matchAll(/^\s*(?:[-*+]|\d+\.)\s+\*\*([^*]+)\*\*/gm)) {
    const li = new Element("li");
    const strong = new Element("strong");
    strong.textContent = match[1];
    li.append(strong);
    app.ui.guideContent.append(li);
  }
  for (const [parentTag, label, prefix] of [["p", ids[0], false], ["code", ids[0], false],
    ["li", "w99.future", false], ["li", ids[0] + "x", false], ["li", ids[0], true]]) {
    const parent = new Element(parentTag);
    const strong = new Element("strong");
    strong.textContent = label;
    if (prefix) parent.append(new Element("#text"));
    parent.append(strong);
    app.ui.guideContent.append(parent);
  }
  app.enhanceGuideCourses();
  const buttons = app.ui.guideContent.querySelectorAll(".guide-task-button");
  assert.equal(buttons.length, 23);
  assert.deepEqual(buttons.map((button) => button.dataset.taskId), ids);
  app.enhanceGuideCourses();
  assert.equal(app.ui.guideContent.querySelectorAll(".guide-task-button").length, 23, "enhancement is idempotent");
});

test("guide course controls become available when the catalog arrives after guide content", async () => {
  const app = application();
  app.state.tasks = [];
  app.state.task = null;
  app.setApi(async (url) => url.endsWith("learning-guide") ? { markdown: "- **w1.hello-world**：第一课" }
    : { tasks: [task("w1.hello-world", "AVAILABLE")] });
  await app.openLearningGuide(app.ui.guideButton);
  const li = new Element("li");
  const strong = new Element("strong");
  strong.textContent = "w1.hello-world";
  li.append(strong);
  app.ui.guideContent.append(li);
  app.enhanceGuideCourses();
  assert.equal(app.ui.guideContent.querySelectorAll(".guide-task-button").length, 0);
  await app.loadTasks();
  assert.equal(app.ui.guideContent.querySelectorAll(".guide-task-button").length, 1);
});

test("guide section shortcuts focus a heading and scroll locally without changing the lesson URL", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.hello-world" });
  app.setApi(async () => ({ markdown: "## 常用词解释" }));
  await app.openLearningGuide(app.ui.guideButton);
  const heading = new Element("h2");
  heading.textContent = "常用词解释";
  heading.top = 340;
  app.ui.guideContent.top = 100;
  app.ui.guideContent.append(heading);
  const originalUrl = app.location.href;
  app.jumpToGuideSection("常用词解释");
  assert.equal(heading.focusCalls, 1);
  assert.equal(heading.attributes.tabindex, "-1");
  assert.ok(app.ui.guideContent.scrollTop > 0);
  assert.equal(app.location.href, originalUrl);
});

test("guide course selection opens a locked lesson through the normal read path", async () => {
  const app = application();
  const requests = [];
  app.setApi(async (url, options = {}) => {
    requests.push([url, options.method || "GET"]);
    if (url.endsWith("learning-guide")) return { markdown: "# 学习路线" };
    if (url === "/api/v1/tasks/w1.calculator") return task("w1.calculator", "LOCKED", ["w1.hello-world"]);
    throw new Error("unexpected request: " + url);
  });
  await app.openLearningGuide(app.ui.welcomeGuide);
  assert.equal(await app.selectGuideTask("w1.calculator"), true);
  assert.equal(app.state.task.id, "w1.calculator");
  assert.equal(app.ui.guideDialog.open, false);
  assert.equal(app.ui.startButton.disabled, true);
  assert.equal(app.ui.taskTitle.focusCalls, 1);
  assert.deepEqual(requests, [["/api/v1/learning-guide", "GET"], ["/api/v1/tasks/w1.calculator", "GET"]]);
});

test("guide course selection stays readable and makes no request while the workbench is busy", async () => {
  const app = application();
  let reads = 0;
  app.setApi(async () => { reads += 1; return { markdown: "# 指南" }; });
  await app.openLearningGuide(app.ui.guideButton);
  app.state.busy = true;
  assert.equal(await app.selectGuideTask("w1.calculator"), false);
  assert.equal(reads, 1);
  assert.equal(app.state.task.id, "w1.hello-world");
  assert.equal(app.ui.guideDialog.open, true);
  assert.match(app.ui.guideStatus.textContent, /等待/);
});

test("guide course selection retains dirty code and the dialog when saving fails", async () => {
  const app = application();
  const requests = [];
  app.setApi(async (url, options = {}) => {
    requests.push([url, options.method || "GET"]);
    if (url.endsWith("learning-guide")) return { markdown: "# 指南" };
    if (options.method === "PUT") throw new Error("disk full");
    throw new Error("unexpected request: " + url);
  });
  await app.openLearningGuide(app.ui.guideButton);
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "// keep this draft";
  const originalUrl = app.location.href;
  assert.equal(await app.selectGuideTask("w1.calculator"), false);
  assert.equal(app.state.task.id, "w1.hello-world");
  assert.equal(app.ui.editor.value, "// keep this draft");
  assert.equal(app.state.dirtyFiles.has("src/main.cpp"), true);
  assert.equal(app.location.href, originalUrl);
  assert.equal(app.ui.guideDialog.open, true);
  assert.match(app.ui.guideStatus.textContent, /未能保存/);
  assert.equal(requests.some(([url]) => url === "/api/v1/tasks/w1.calculator"), false);
});

test("a pending guide course selection cannot close or focus a subsequently reopened guide", async () => {
  const app = application();
  let release;
  app.setApi(async (url) => {
    if (url.endsWith("learning-guide")) return { markdown: "# 指南" };
    return new Promise((resolve) => { release = resolve; });
  });
  await app.openLearningGuide(app.ui.guideButton);
  const selecting = app.selectGuideTask("w1.calculator");
  app.closeLearningGuide();
  await app.openLearningGuide(app.ui.guideButton);
  const focusCalls = app.ui.guideClose.focusCalls;
  release(task("w1.calculator", "LOCKED", ["w1.hello-world"]));
  assert.equal(await selecting, true);
  assert.equal(app.ui.guideDialog.open, true);
  assert.equal(app.ui.guideClose.focusCalls, focusCalls);
  assert.equal(app.ui.taskTitle.focusCalls, 0);
});

for (const reopen of [false, true]) {
  test("a late guide workspace read does not focus the editor after " + (reopen ? "reopening" : "closing") + " help", async () => {
    const app = application();
    let release;
    let received;
    const sourceRequested = new Promise((resolve) => { received = resolve; });
    app.setApi(async (url) => {
      if (url.endsWith("learning-guide")) return { markdown: "# 指南" };
      if (url === "/api/v1/tasks/w1.calculator") return task("w1.calculator", "IN PROGRESS");
      if (url === "/api/v1/workspaces/w1.calculator/files") return { files: [{ path: "src/main.cpp", read_only: false }] };
      if (url === "/api/v1/workspaces/w1.calculator/files/src/main.cpp") {
        received();
        return new Promise((resolve) => { release = resolve; });
      }
      throw new Error("unexpected request: " + url);
    });
    await app.openLearningGuide(app.ui.guideButton);
    const selecting = app.selectGuideTask("w1.calculator");
    await sourceRequested;
    app.closeLearningGuide();
    if (reopen) await app.openLearningGuide(app.ui.guideButton);
    else app.ui.search.focus();
    const guideFocus = app.ui.guideClose.focusCalls;
    release({ content: "// restored workspace" });
    assert.equal(await selecting, true);
    assert.equal(app.ui.editor.value, "// restored workspace");
    assert.equal(app.state.activeFile, "src/main.cpp");
    assert.equal(app.ui.guideDialog.open, reopen);
    assert.equal(app.ui.guideClose.focusCalls, guideFocus);
    assert.equal(app.ui.taskTitle.focusCalls, 0);
    assert.equal(app.ui.editor.focusCalls, 0, "guide-owned navigation must not implicitly focus its loaded editor");
  });
}

test("the welcome guide entry returns to first-use instructions while the topbar retains reading position", async () => {
  const app = application();
  let reads = 0;
  app.setApi(async () => { reads += 1; return { markdown: "## 第一次使用：先完成一个最小任务" }; });
  await app.openLearningGuide(app.ui.guideButton);
  const firstUse = new Element("h2");
  firstUse.textContent = "第一次使用：先完成一个最小任务";
  firstUse.top = -560;
  app.ui.guideContent.top = 100;
  app.ui.guideContent.scrollTop = 860;
  app.ui.guideContent.append(firstUse);
  app.closeLearningGuide();
  await app.openLearningGuide(app.ui.guideButton);
  assert.equal(app.ui.guideContent.scrollTop, 860, "the persistent entry keeps the current reading position");
  app.closeLearningGuide();
  await app.openLearningGuide(app.ui.welcomeGuide);
  assert.equal(app.ui.guideContent.scrollTop, 188, "the first-use entry must show its promised four steps");
  assert.equal(firstUse.focusCalls, 0, "opening the dialog keeps its close control focused");
  assert.equal(reads, 1, "finding the first-use section does not refetch the guide");
});

test("closing the learning guide restores a visible control after initial navigation hides its welcome opener", async () => {
  const app = application();
  app.setApi(async () => ({ markdown: "# 指南" }));
  await app.openLearningGuide(app.ui.welcomeGuide);
  app.ui.welcome.hidden = true;
  app.closeLearningGuide();
  assert.equal(app.ui.guideButton.focusCalls, 1);
  assert.equal(app.ui.welcomeGuide.focusCalls, 0, "a hidden welcome button cannot receive focus");
});

test("busy course selection keeps acceptance attached to its original task", async () => {
  const app = application();
  app.state.busy = true;
  let reads = 0;
  app.setApi(async () => { reads += 1; return task("w2.todo-cli", "LOCKED", ["w1.calculator"]); });
  await app.selectTask("w2.todo-cli");
  assert.equal(app.state.selectedTaskId, "w1.hello-world");
  assert.equal(app.state.task.id, "w1.hello-world");
  assert.equal(reads, 0, "busy navigation must not even start a different task request");
});

test("saving accepted source refreshes dependent courses and visible history", async () => {
  const app = application();
  app.state.task.state = "PASSED";
  app.state.tasks[1].state = "AVAILABLE";
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "// changed source";
  app.ui.historyPanel.hidden = false;
  const requests = [];
  app.setApi(async (url, options = {}) => {
    requests.push([url, options.method || "GET"]);
    if (options.method === "PUT") return { saved: true };
    if (url === "/api/v1/tasks") return { tasks: [task("w1.hello-world", "STALE"), task("w1.calculator", "LOCKED", ["w1.hello-world"])] };
    if (url.endsWith("/history")) return { count: 1, current_state: "STALE",
      records: [{ timestamp: "2026-11-18", result: "PASS", tests: "1/1", workspace: "fixture", fingerprint: "old" }] };
    throw new Error("unexpected request: " + url);
  });
  assert.equal(await app.saveFile("src/main.cpp", { silent: true }), true);
  assert.equal(app.state.tasks.find((item) => item.id === "w1.calculator").state, "LOCKED");
  assert.match(app.ui.historySummary.textContent, /结果已过期/);
  assert.equal(app.ui.historyList.children.length, 1, "historical record remains present");
  assert.equal(requests.filter((item) => item[0] === "/api/v1/tasks").length, 1);
});

test("course switching owns the busy state while saving the outgoing file", async () => {
  const app = application();
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "unsaved code";
  let release;
  const gate = new Promise((resolve) => { release = resolve; });
  app.setApi(async (url, options = {}) => {
    if (options.method === "PUT") { await gate; return { saved: true }; }
    return task("w1.calculator", "LOCKED", ["w1.hello-world"]);
  });
  const pending = app.selectTask("w1.calculator");
  try {
    assert.equal(app.state.busy, true, "navigation must lock before its first asynchronous save");
    await app.selectTask("w2.todo-cli");
  } finally { release(); await pending; }
  assert.equal(app.state.selectedTaskId, "w1.calculator");
});

test("references cannot enter the edit, Tab or save path", async () => {
  const app = application();
  app.state.files.push({ path: "include/task.hpp", read_only: true });
  let writes = 0;
  app.setApi(async () => { writes += 1; return {}; });
  app.setEditorValue("include/task.hpp", "interface declaration");
  assert.equal(app.ui.editor.readOnly, true);
  app.handleEditorInput();
  app.bindEvents();
  app.ui.editor.fire("keydown", { key: "Tab", preventDefault() {} });
  assert.equal(app.ui.editor.value, "interface declaration");
  assert.equal(app.state.dirtyFiles.size, 0);
  assert.equal(await app.saveFile("include/task.hpp"), false);
  assert.equal(writes, 0);
  app.setEditorValue("src/main.cpp", "editable");
  assert.equal(app.ui.editor.readOnly, false, "returning to learner source must restore editing");
});

test("overlapping saves serialize one file and retain edits made during the request", async () => {
  const app = application();
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.state.fileVersions.set("src/main.cpp", 1);
  app.ui.editor.value = "draft one";
  let release;
  const gate = new Promise((resolve) => { release = resolve; });
  const writes = [];
  app.setApi(async (url, options) => { writes.push(options.body.content); await gate; return { saved: true }; });
  const first = app.saveFile("src/main.cpp");
  app.ui.editor.value = "draft two";
  app.handleEditorInput();
  const second = app.saveFile("src/main.cpp");
  try { assert.equal(writes.length, 1, "concurrent saves must not race writes to the same file"); }
  finally { release(); await Promise.all([first, second]); }
  assert.equal(app.state.dirtyFiles.has("src/main.cpp"), true, "newer input still requires saving");
  await app.saveFile("src/main.cpp");
  assert.deepEqual(writes, ["draft one", "draft two"]);
  assert.equal(app.state.dirtyFiles.size, 0);
});

test("a late hint response cannot appear under a different course", async () => {
  const app = application();
  let release;
  app.setApi(() => new Promise((resolve) => { release = resolve; }));
  const pending = app.showHint(3);
  app.state.task = task("w2.todo-cli", "LOCKED");
  app.state.selectedTaskId = "w2.todo-cli";
  release({ markdown: "Wrong course hint body" });
  await pending;
  assert.doesNotMatch(app.ui.hint.innerHTML, /Wrong course hint body/);
});

test("continuation uses refreshed formal progress and hides after refresh failure", async () => {
  const app = application();
  app.state.task.state = "PASSED";
  app.setApi(async () => ({ tasks: [task("w1.hello-world", "PASSED"), task("w1.calculator", "AVAILABLE", ["w1.hello-world"])] }));
  await app.loadTasks();
  assert.equal(app.byId("workflow-action").dataset.taskId, "w1.calculator");
  app.setApi(async () => { throw new Error("temporarily unavailable"); });
  await assert.rejects(app.loadTasks(), /temporarily unavailable/);
  assert.equal(app.byId("workflow-action").dataset.taskId, undefined,
    "a stale continuation must not survive a failed progress refresh");
  assert.match(app.byId("workflow-help").textContent, /刷新/);
});

test("out-of-order history reads retain the newest current-source state", async () => {
  const app = application();
  const replies = [];
  app.setApi(() => new Promise((resolve) => { replies.push(resolve); }));
  const previous = app.loadTaskHistory("w1.hello-world");
  const current = app.loadTaskHistory("w1.hello-world");
  const record = { timestamp: "2026-11-18", result: "PASS", tests: "1/1", workspace: "fixture", fingerprint: "old" };
  replies[1]({ count: 1, current_state: "STALE", records: [record] });
  await current;
  replies[0]({ count: 1, current_state: "PASSED", records: [record] });
  await previous;
  assert.match(app.ui.historySummary.textContent, /结果已过期/);
  assert.equal(app.ui.historyList.children.length, 1, "late history must not append duplicate records");
});

test("offline compiler diagnostics preserve the real failure and acceptance state", async () => {
  const app = application();
  const output = "/workspace/src/main.cpp:2:4: error: expected semicolon\n";
  app.setApi(async (url) => {
    if (url.endsWith("/check")) return { passed: false, output, exit_code: 1 };
    if (url === "/api/v1/tasks") return { tasks: [task("w1.hello-world", "NEEDS WORK")] };
    throw new Error("unexpected request: " + url);
  });
  await app.runWorkspaceAction("check");
  assert.equal(app.ui.console.textContent, output);
  assert.equal(app.state.task.state, "NEEDS WORK");
  assert.equal(app.ui.diagnostics.children.length, 1);
  assert.equal(app.ui.diagnostics.children[0].disabled, false);
});

test("acceptance success keeps a failed progress refresh visible", async () => {
  const app = application();
  app.setApi(async (url) => {
    if (url.endsWith("/check")) return { passed: true, output: "[PASS] fixed acceptance", exit_code: 0 };
    throw new Error("progress unavailable");
  });
  await app.runWorkspaceAction("check");
  assert.equal(app.state.task.state, "PASSED", "the completed formal check remains recorded");
  assert.match(app.ui.toast.textContent, /刷新失败/);
  assert.match(app.byId("workflow-help").textContent, /刷新/);
  assert.equal(app.byId("workflow-action").dataset.taskId, undefined);
});

test("out-of-order catalog reads cannot restore an obsolete continuation", async () => {
  const app = application();
  const replies = [];
  app.setApi(() => new Promise((resolve) => { replies.push(resolve); }));
  const previous = app.loadTasks();
  const current = app.loadTasks();
  replies[1]({ tasks: [task("w1.hello-world", "STALE"), task("w1.calculator", "LOCKED")] });
  await current;
  replies[0]({ tasks: [task("w1.hello-world", "PASSED"), task("w1.calculator", "AVAILABLE")] });
  await previous;
  assert.equal(app.state.task.state, "STALE");
  assert.equal(app.state.tasks[1].state, "LOCKED");
  assert.equal(app.ui.workflowAction.dataset.taskId, undefined);
});

test("a superseded catalog error cannot invalidate a newer successful refresh", async () => {
  const app = application();
  const replies = [];
  app.setApi(() => new Promise((resolve, reject) => { replies.push({ resolve, reject }); }));
  const previous = app.loadTasks();
  const current = app.loadTasks();
  replies[1].resolve({ tasks: [task("w1.hello-world", "PASSED"), task("w1.calculator", "AVAILABLE")] });
  await current;
  replies[0].reject(new Error("old request failed"));
  await previous;
  assert.equal(app.state.progressFresh, true);
  assert.equal(app.ui.workflowAction.dataset.taskId, "w1.calculator");
});

test("unknown progress cannot be presented as a completed curriculum", async () => {
  const app = application();
  app.setApi(async () => ({ tasks: [task("w1.hello-world", "UNKNOWN"), task("w1.calculator", "UNKNOWN")] }));
  await app.loadTasks().catch(() => {});
  app.setBusy(false);
  assert.equal(app.state.progressFresh, false);
  assert.doesNotMatch(app.ui.continueButton.textContent, /课程已完成/);
  assert.equal(app.ui.continueButton.disabled, true);
  assert.equal(app.ui.startButton.disabled, true);
  assert.equal(app.ui.workflowAction.dataset.action, "refresh");
});

test("progress recovery also refreshes the history currently visible to the learner", async () => {
  const app = application();
  app.ui.historyPanel.hidden = false;
  app.ui.historySummary.textContent = "当前状态：已通过";
  app.ui.workflowAction.dataset.action = "refresh";
  app.setApi(async (url) => {
    if (url === "/api/v1/tasks") return { tasks: [task("w1.hello-world", "STALE")] };
    if (url.endsWith("/history")) return { count: 0, current_state: "STALE", records: [] };
    throw new Error("unexpected request: " + url);
  });
  await app.followWorkflowAction();
  assert.match(app.ui.historySummary.textContent, /结果已过期/);
});

test("restoring the accepted source refreshes PASS without adding an acceptance", async () => {
  const app = application();
  app.state.task.state = "STALE";
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "accepted original source";
  app.ui.historyPanel.hidden = false;
  app.setApi(async (url, options = {}) => {
    if (options.method === "PUT") return { saved: true };
    if (url === "/api/v1/tasks") return { tasks: [task("w1.hello-world", "PASSED"), task("w1.calculator", "AVAILABLE")] };
    if (url.endsWith("/history")) return { count: 1, current_state: "PASSED", records: [
      { timestamp: "2026-11-18", result: "PASS", tests: "1/1", workspace: "fixture", fingerprint: "accepted" }
    ] };
    throw new Error("unexpected request: " + url);
  });
  assert.equal(await app.saveFile("src/main.cpp", { silent: true }), true);
  assert.equal(app.state.task.state, "PASSED");
  assert.equal(app.ui.workflowAction.dataset.taskId, "w1.calculator");
  assert.match(app.ui.historySummary.textContent, /共 1 次正式验收.*已通过/);
});

test("late service status cannot populate a different course", async () => {
  const app = application();
  app.state.task.service_profile = { kind: "tcp_echo" };
  let release;
  app.setApi(() => new Promise((resolve) => { release = resolve; }));
  const pending = app.refreshServiceStatus();
  app.state.task = task("w8.http-server", "IN PROGRESS");
  app.state.task.service_profile = { kind: "http" };
  const current = { state: "STOPPED", port: null, logs: "current course" };
  app.state.serviceSnapshot = current;
  release({ state: "RUNNING", port: 8123, logs: "old course" });
  await pending;
  assert.equal(app.state.serviceSnapshot, current);
});

test("a service status started before stop cannot revive the stopped controls", async () => {
  const app = application();
  app.state.task.service_profile = { kind: "tcp_echo" };
  let release;
  app.setApi(async (url) => {
    if (url.endsWith("/status")) return new Promise((resolve) => { release = resolve; });
    if (url.endsWith("/stop")) return { state: "STOPPED", port: null, logs: "stopped" };
    throw new Error("unexpected request: " + url);
  });
  const pending = app.refreshServiceStatus();
  await app.stopNetworkService();
  release({ state: "RUNNING", port: 8123, logs: "before stop" });
  await pending;
  assert.equal(app.state.serviceSnapshot.state, "STOPPED");
  assert.equal(app.ui.serviceStopButton.disabled, true);
});

function learningApi(app, extra = () => undefined) {
  const requests = [];
  app.setApi(async (url, options = {}) => {
    requests.push(url);
    const result = extra(url, options);
    if (result !== undefined) return result;
    if (url.endsWith("/session")) return { csrf_token: "test" };
    if (url.endsWith("/health")) return { status: "ok" };
    if (url === "/api/v1/tasks") return { tasks: [task("w1.hello-world", "PASSED"), task("w1.calculator", "IN PROGRESS")] };
    if (url === "/api/v1/tasks/w1.hello-world") return task("w1.hello-world", "PASSED");
    if (url === "/api/v1/tasks/w1.calculator") return task("w1.calculator", "IN PROGRESS");
    if (url.endsWith("/files")) return { files: [
      { path: "include/calculator.hpp", read_only: false }, { path: "src/calculator.cpp", read_only: false }
    ] };
    if (url.includes("/files/")) return { content: "// saved learner source\n", read_only: false };
    throw new Error("unexpected request: " + url);
  });
  return requests;
}

test("startup restores an explicit course and non-first file from its URL", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.calculator&file=src%2Fcalculator.cpp" });
  app.state.task = null;
  app.state.selectedTaskId = "";
  const requests = learningApi(app);
  await app.init();
  assert.equal(app.state.task?.id, "w1.calculator");
  assert.equal(app.state.activeFile, "src/calculator.cpp");
  assert.equal(app.ui.editor.value, "// saved learner source\n");
  assert.equal(requests.some((url) => url.endsWith("/start")), false, "restoration must not create a workspace");
});

test("a failed destination read preserves the current course and editor", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.hello-world&file=src%2Fmain.cpp" });
  app.state.activeFile = "src/main.cpp";
  app.ui.editor.value = "saved hello";
  app.ui.lesson.innerHTML = "<h1>Original lesson</h1>";
  const previous = app.state.task;
  app.setApi(async () => { throw new Error("destination temporarily unavailable"); });
  await app.selectTask("w1.calculator");
  assert.equal(app.state.task, previous);
  assert.equal(app.state.selectedTaskId, "w1.hello-world");
  assert.equal(app.state.activeFile, "src/main.cpp");
  assert.equal(app.ui.editor.value, "saved hello");
  assert.equal(app.ui.lesson.innerHTML, "<h1>Original lesson</h1>");
  assert.match(app.location.hash, /task=w1.hello-world/);
});

test("a workspace read failure retains the lesson and offers a read-only retry", async () => {
  const app = application();
  app.state.progressFresh = true;
  let fail = true;
  const requests = learningApi(app, (url) => {
    if (fail && url.endsWith("/files")) throw new Error("workspace temporarily unavailable");
  });
  await app.selectTask("w1.calculator");
  assert.doesNotMatch(app.ui.lesson.textContent, /workspace temporarily unavailable/);
  assert.match(app.ui.lesson.innerHTML, /Lesson/);
  assert.equal(app.ui.workflowAction.dataset.action, "workspace");
  fail = false;
  await app.retryWorkspaceFiles();
  assert.equal(app.state.workspaceReady, true);
  assert.equal(app.state.workspaceError, "");
  assert.match(app.ui.lesson.innerHTML, /Lesson/);
  assert.equal(requests.some((url) => url.endsWith("/start")), false);
});

test("revisiting a course returns to its last successfully opened file", async () => {
  const app = application();
  app.state.progressFresh = true;
  learningApi(app);
  await app.selectTask("w1.calculator");
  await app.loadFile("src/calculator.cpp");
  await app.selectTask("w1.hello-world");
  await app.selectTask("w1.calculator");
  assert.equal(app.state.activeFile, "src/calculator.cpp");
  assert.equal(new URLSearchParams(app.location.hash.slice(1)).get("file"), "src/calculator.cpp");
  assert.match(app.byId("resume-button").textContent, /Calculator/);
  assert.equal(app.byId("resume-button").hidden, false);
});

test("a busy incoming URL cannot switch courses or leave a misleading address", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.hello-world&file=src%2Fmain.cpp" });
  app.state.activeFile = "src/main.cpp";
  app.bindEvents();
  app.state.busy = true;
  app.location.hash = "task=w1.calculator";
  await app.browserEvent("hashchange");
  assert.equal(app.state.task.id, "w1.hello-world");
  assert.match(app.location.hash, /task=w1.hello-world/);
});

test("invalid file links never read an unlisted path and blocked storage is harmless", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.calculator&file=..%2Fsecret", storageDenied: true });
  app.state.task = null;
  app.state.selectedTaskId = "";
  const requests = learningApi(app);
  await app.init();
  assert.equal(app.state.task?.id, "w1.calculator");
  assert.equal(app.state.activeFile, "include/calculator.hpp");
  assert.equal(requests.some((url) => url.includes("secret")), false);
  assert.equal(app.state.busy, false);
});

test("a failed outgoing save keeps the original route and source", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.hello-world&file=src%2Fmain.cpp" });
  app.state.activeFile = "src/main.cpp";
  app.state.dirtyFiles.add("src/main.cpp");
  app.ui.editor.value = "unsaved draft";
  app.bindEvents();
  app.setApi(async () => { throw new Error("save unavailable"); });
  app.location.hash = "task=w1.calculator";
  await app.browserEvent("hashchange");
  assert.equal(app.state.task.id, "w1.hello-world");
  assert.equal(app.ui.editor.value, "unsaved draft");
  assert.equal(app.state.dirtyFiles.has("src/main.cpp"), true);
  assert.match(app.location.hash, /task=w1.hello-world/);
});

test("the welcome return action reopens stored presentation context only", async () => {
  const app = application({ storage: { "cppapplab.learning-position.v1": JSON.stringify({
    version: 1, lastTaskId: "w1.calculator", files: { "w1.calculator": "src/calculator.cpp" }
  }) } });
  app.state.task = null;
  app.state.selectedTaskId = "";
  const requests = learningApi(app);
  await app.init();
  assert.equal(app.ui.welcome.hidden, false, "the root URL remains a welcome page");
  assert.match(app.ui.resumeButton.textContent, /Calculator/);
  app.ui.resumeButton.fire("click");
  await new Promise(setImmediate);
  assert.equal(app.state.task?.id, "w1.calculator");
  assert.equal(app.state.activeFile, "src/calculator.cpp");
  assert.equal(requests.some((url) => url.endsWith("/start")), false);
  assert.doesNotMatch(app.stored.get("cppapplab.learning-position.v1"), /saved learner source/);
});

test("an unknown task bookmark never reaches a task endpoint", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=unknown.course&file=src%2Fmain.cpp" });
  app.state.task = null;
  app.state.selectedTaskId = "";
  const requests = learningApi(app);
  await app.init();
  assert.equal(app.state.task, null);
  assert.equal(app.location.hash, "");
  assert.equal(requests.some((url) => url.includes("unknown.course")), false);
  assert.match(app.ui.toast.textContent, /链接中的课程不可用/);
});

test("corrupt recent-position storage does not hide the available course path", async () => {
  const app = application({ storage: { "cppapplab.learning-position.v1": "{broken JSON" } });
  app.state.task = null;
  app.state.selectedTaskId = "";
  learningApi(app);
  await app.init();
  assert.equal(app.ui.resumeButton.hidden, true);
  assert.equal(app.ui.continueButton.disabled, false);
  assert.equal(app.state.progressFresh, true);
});

test("a course-only bookmark reuses that course's remembered file", async () => {
  const app = application({ url: "http://127.0.0.1:8765/#task=w1.calculator", storage: {
    "cppapplab.learning-position.v1": JSON.stringify({ version: 1, lastTaskId: "w1.calculator",
      files: { "w1.calculator": "src/calculator.cpp" } })
  } });
  app.state.task = null;
  app.state.selectedTaskId = "";
  learningApi(app);
  await app.init();
  assert.equal(app.state.activeFile, "src/calculator.cpp");
});

test("same-course file fragments preserve the editor and running service when the file read fails", async () => {
  const app = application();
  app.bindEvents();
  app.state.progressFresh = true;
  let fail = true;
  const requests = learningApi(app, (url) => {
    if (fail && url.endsWith("/files/src/calculator.cpp")) throw new Error("file unavailable");
    if (url.endsWith("/service/stop")) return { state: "STOPPED" };
  });
  await app.selectTask("w1.calculator");
  const current = app.state.task;
  current.service_profile = { kind: "tcp_echo" };
  app.state.serviceSnapshot = { state: "RUNNING", port: 8123, logs: "running" };
  app.state.lastAction = { taskId: current.id, action: "check", passed: true };
  const priorUrl = app.location.href;
  const priorSource = app.ui.editor.value;
  const start = requests.length;
  app.location.hash = "task=w1.calculator&file=src%2Fcalculator.cpp";
  await app.browserEvent("hashchange");
  assert.equal(app.state.task, current);
  assert.equal(app.state.activeFile, "include/calculator.hpp");
  assert.equal(app.ui.editor.value, priorSource);
  assert.equal(app.location.href, priorUrl);
  assert.equal(app.state.workspaceReady, true);
  assert.equal(app.state.serviceSnapshot.state, "RUNNING");
  assert.equal(app.state.lastAction.action, "check");
  assert.deepEqual(requests.slice(start), ["/api/v1/workspaces/w1.calculator/files/src/calculator.cpp"]);
  fail = false;
  app.location.hash = "task=w1.calculator&file=src%2Fcalculator.cpp";
  await app.browserEvent("hashchange");
  assert.equal(app.state.activeFile, "src/calculator.cpp");
  assert.equal(app.state.serviceSnapshot.state, "RUNNING");
});

test("failed initial source reads keep file controls guarded until the explicit workspace retry", async () => {
  const app = application();
  app.state.progressFresh = true;
  let fail = true;
  const requests = learningApi(app, (url) => {
    if (fail && url.includes("/files/")) throw new Error("initial source unavailable");
  });
  await app.selectTask("w1.calculator");
  assert.equal(app.state.workspaceReady, false);
  assert.ok(app.ui.fileList.children.every((button) => button.disabled));
  fail = false;
  const before = requests.length;
  assert.equal(await app.loadFile("src/calculator.cpp"), false);
  assert.equal(requests.length, before, "file links and diagnostics cannot bypass the workspace retry");
  assert.equal(app.state.activeFile, "");
  assert.equal(app.ui.editor.readOnly, true);
  assert.equal(await app.retryWorkspaceFiles(), true);
  assert.equal(app.state.workspaceReady, true);
  assert.equal(app.state.workspaceError, "");
  assert.ok(app.ui.fileList.children.every((button) => !button.disabled));
  await app.loadFile("src/calculator.cpp");
  app.ui.editor.value = "// now editable and saveable";
  app.handleEditorInput();
  assert.equal(await app.saveFile("src/calculator.cpp"), true);
});

for (const blockedBy of ["busy", "failed save"]) {
  test("rejected Back preserves visited entries and can be retried after " + blockedBy, async () => {
    const app = application();
    app.bindEvents();
    app.state.progressFresh = true;
    let rejectSave = false;
    learningApi(app, (_url, options) => {
      if (options.method === "PUT" && rejectSave) throw new Error("save unavailable");
    });
    await app.selectTask("w1.hello-world");
    await app.selectTask("w1.calculator");
    const priorEntries = [...app.entries];
    if (blockedBy === "busy") app.setBusy(true);
    else {
      app.ui.editor.value = "// unsaved current source";
      app.handleEditorInput();
      rejectSave = true;
    }
    await app.travel(-1);
    assert.equal(app.state.task.id, "w1.calculator");
    assert.deepEqual(app.entries, priorEntries, "rejected traversal must not rewrite the previous course entry");
    assert.equal(new URLSearchParams(app.location.hash.slice(1)).get("task"), "w1.calculator");
    app.setBusy(false);
    rejectSave = false;
    await app.travel(-1);
    assert.equal(app.state.task.id, "w1.hello-world");
    await app.travel(1);
    assert.equal(app.state.task.id, "w1.calculator");
  });
}

test("a pending bookmarked file survives unavailable workspace reads and refresh before Start", async () => {
  for (const available of [true, false]) {
    const app = application({ url: "http://127.0.0.1:8765/#task=w1.calculator&file=src%2Fcalculator.cpp" });
    app.state.task = null;
    learningApi(app, (url) => {
      if (available && url === "/api/v1/tasks/w1.calculator") return task("w1.calculator", "AVAILABLE");
      if (!available && url.endsWith("/files")) throw new Error("workspace unavailable");
    });
    await app.init();
    assert.equal(new URLSearchParams(app.location.hash.slice(1)).get("file"), "src/calculator.cpp");
    const refreshed = application({ url: app.location.href });
    refreshed.state.task = null;
    learningApi(refreshed);
    await refreshed.init();
    assert.equal(refreshed.state.activeFile, "src/calculator.cpp");
  }
});

test("Back during a pending course read preserves the previous committed entry", async () => {
  const app = application();
  app.bindEvents();
  app.state.progressFresh = true;
  learningApi(app);
  await app.selectTask("w1.hello-world");
  const priorEntries = [...app.entries];
  let release;
  const gate = new Promise((resolve) => { release = resolve; });
  learningApi(app, (url) => {
    if (url === "/api/v1/workspaces/w1.calculator/files") return gate.then(() => ({
      files: [{ path: "src/calculator.cpp", read_only: false }]
    }));
  });
  const pending = app.selectTask("w1.calculator");
  await new Promise(setImmediate);
  assert.equal(app.state.task.id, "w1.calculator");
  try {
    await app.travel(-1);
    assert.deepEqual(app.entries, priorEntries, "rollback must not write the half-loaded destination into the previous entry");
  } finally { release(); await pending; }
  await app.travel(-1);
  assert.equal(app.state.task.id, "w1.hello-world");
  await app.travel(1);
  assert.equal(app.state.task.id, "w1.calculator");
});
