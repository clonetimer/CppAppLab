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
  }
  append(...items) { this.children.push(...items); }
  replaceChildren(...items) { this.children = items; }
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
    if (selector === ".task-link") return all.filter((child) => child.className?.split(" ").includes("task-link"));
    return [];
  }
  querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
  focus() {}
  scrollIntoView() {}
  scrollTo() {}
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
  const document = {
    getElementById: byId, createElement: (tag) => new Element(tag), addEventListener() {},
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
  async function flushHistory() {
    while (pendingHistory.length) await Promise.all(pendingHistory.splice(0));
  }
  return { ...app, byId, location, stored, entries,
    async travel(delta) { window.history.go(delta); await flushHistory(); },
    async browserEvent(name) {
      await Promise.all((listeners[name] || []).map((handler) => handler()));
      await flushHistory();
    } };
}

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
