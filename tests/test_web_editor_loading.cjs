"use strict";

// Run the real editor startup against controlled DOM, clock and CDN boundaries.
// Native Monaco rendering and browser layout remain covered by Chromium in CI.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const test = require("node:test");

class Element {
  constructor(tag = "div", markup = "") {
    this.tagName = tag.toUpperCase();
    this.children = [];
    this.dataset = {};
    this.listeners = {};
    this.classList = { add() {}, remove() {}, toggle() {} };
    this.hidden = /\shidden(?:\s|>)/.test(markup);
    this.disabled = /\sdisabled(?:\s|>)/.test(markup);
    this.readOnly = false;
    this.textContent = "";
    this.value = "";
  }
  append(...children) { this.children.push(...children); }
  replaceChildren(...children) { this.children = children; }
  addEventListener(name, listener) { (this.listeners[name] ||= []).push(listener); }
  fire(name, event = {}) { for (const listener of this.listeners[name] || []) listener(event); }
  querySelectorAll() { return []; }
  setAttribute() {}
  focus() {}
}

const flushPromises = () => new Promise(setImmediate);

function clock() {
  let now = 0;
  let sequence = 0;
  const timers = new Map();
  return {
    setTimeout(callback, delay) {
      const id = ++sequence;
      timers.set(id, { at: now + delay, callback });
      return id;
    },
    clearTimeout(id) { timers.delete(id); },
    async advance(duration) {
      const until = now + duration;
      while (true) {
        const next = [...timers].filter(([, timer]) => timer.at <= until)
          .sort((a, b) => a[1].at - b[1].at || a[0] - b[0])[0];
        if (!next) break;
        const [id, timer] = next;
        timers.delete(id);
        now = timer.at;
        timer.callback();
        await flushPromises();
      }
      now = until;
      await flushPromises();
    }
  };
}

function observe(promise) {
  const result = { status: "pending" };
  promise.then((value) => Object.assign(result, { status: "resolved", value }),
    (error) => Object.assign(result, { status: "rejected", error }));
  return result;
}

function application() {
  const root = path.join(__dirname, "..", "web", "ui");
  const html = fs.readFileSync(path.join(root, "index.html"), "utf8");
  const elements = new Map([...html.matchAll(/<([a-z][\w-]*)\b[^>]*\bid="([^"]+)"[^>]*>/gi)]
    .map(([markup, tag, id]) => [id, new Element(tag, markup)]));
  const byId = (id) => elements.get(id) || null;
  const document = {
    head: new Element("head"), getElementById: byId,
    createElement: (tag) => new Element(tag), querySelectorAll: () => [], addEventListener() {}
  };
  const timers = clock();
  const window = { addEventListener() {} };
  const warnings = [];
  const context = vm.createContext({ document, window, Map, Set, URL, Headers,
    console: { warn: (...args) => warnings.push(args) },
    setTimeout: timers.setTimeout, clearTimeout: timers.clearTimeout,
    requestAnimationFrame: (callback) => callback() });
  const filename = path.join(root, "app.js");
  const source = fs.readFileSync(filename, "utf8");
  const injected = source.replace(/\n  init\(\);\n\}\)\(\);\s*$/, `
  globalThis.workbench = { state, ui, ensureEditorReady, bindEvents,
    setBusy, setEditorValue, handleEditorInput, saveFile,
    setApi(handler) { api = handler; } };
})();`);
  assert.notEqual(injected, source, "replace only automatic startup in the test adapter");
  vm.runInContext(injected, context, { filename });
  const app = context.workbench;
  app.state.task = { id: "w1.hello-world", title: "W1 · Hello World", state: "IN PROGRESS",
    prerequisites: [], source_files: ["src/main.cpp"], runtime_profile: null, service_profile: null };
  app.state.selectedTaskId = app.state.task.id;
  app.state.tasks = [app.state.task];
  app.state.workspaceReady = true;
  app.state.progressFresh = true;
  app.state.files = [{ path: "src/main.cpp", read_only: false }, { path: "include/task.hpp", read_only: true }];
  return { ...app, byId, timers, window, warnings, scripts: document.head.children };
}

function provideAmd(app) {
  const pending = [];
  const roots = [];
  app.window.require = (_modules, ready, failed) => pending.push({ ready, failed });
  app.window.require.config = (config) => roots.push(config.paths.vs);
  return { pending, roots };
}

function provideMonaco(app) {
  const editors = [];
  app.window.monaco = {
    KeyMod: { CtrlCmd: 2048 }, KeyCode: { KeyS: 49 }, Uri: { parse: (value) => value },
    editor: {
      create() {
        const editor = {
          model: null, options: {}, commands: [],
          onDidChangeModelContent(handler) { this.changed = handler; },
          addCommand(key, handler) { this.commands.push({ key, handler }); },
          setModel(model) { this.model = model; },
          getModel() { return this.model; },
          getValue() { return this.model.getValue(); },
          updateOptions(options) { Object.assign(this.options, options); },
          layout() {}, focus() {}, dispose() {}
        };
        editors.push(editor);
        return editor;
      },
      createModel(value) {
        return { getValue: () => value, setValue(next) { value = next; }, dispose() {} };
      },
      setModelMarkers() {}
    }
  };
  return editors;
}

function assertFallback(app, result) {
  assert.deepEqual(result, { status: "resolved", value: false });
  assert.equal(app.state.editorInstance, null);
  assert.equal(app.ui.editor.hidden, false);
  assert.equal(app.ui.monacoContainer.hidden, true);
  assert.ok(app.byId("editor-loading"), "the loading action must have a visible lifecycle");
  assert.equal(app.byId("editor-loading").hidden, true);
}

test("a stalled loader releases every waiter at the 8000 ms total deadline", async () => {
  const app = application();
  const first = observe(app.ensureEditorReady());
  const second = observe(app.ensureEditorReady());
  assert.equal(app.scripts.length, 1, "overlapping file loads share editor startup");
  await app.timers.advance(7999);
  assert.equal(first.status, "pending", "a healthy CDN still has its full loading window");
  await app.timers.advance(1);
  assertFallback(app, first);
  assert.deepEqual(second, first);
  assert.equal(await app.ensureEditorReady(), false, "fallback is the final result for this page");
  assert.equal(app.scripts.length, 1, "revisiting a file must not restart a settled loader");
});

test("an already available AMD loader cannot wait forever for editor modules", async () => {
  const app = application();
  provideAmd(app);
  const result = observe(app.ensureEditorReady());
  await app.timers.advance(8000);
  assertFallback(app, result);
});

test("CDN retry and AMD loading share the original deadline", async () => {
  const app = application();
  const result = observe(app.ensureEditorReady());
  await app.timers.advance(5000);
  app.scripts[0].onerror();
  assert.equal(app.scripts.length, 2, "an immediate CDN error still tries the next CDN");
  await app.timers.advance(2000);
  provideAmd(app);
  app.scripts[1].onload();
  await app.timers.advance(999);
  assert.equal(result.status, "pending");
  await app.timers.advance(1);
  assertFallback(app, result);
});

test("the loading action immediately releases the busy file load", async () => {
  const app = application();
  app.bindEvents();
  const loading = app.byId("editor-loading");
  const button = app.byId("editor-fallback-button");
  assert.ok(loading && button, "editor loading must render an immediate basic-editor action");
  assert.equal(loading.hidden, true, "the action is absent before editor initialization");
  app.setBusy(true);
  const result = observe(app.ensureEditorReady());
  assert.equal(loading.hidden, false);
  assert.equal(button.disabled, false, "the recovery action remains usable during a busy course load");
  button.fire("click");
  await flushPromises();
  assertFallback(app, result);
});

test("late script completion cannot replace source edited after manual fallback", async () => {
  const app = application();
  app.bindEvents();
  const result = observe(app.ensureEditorReady());
  const lateLoad = app.scripts[0].onload;
  const button = app.byId("editor-fallback-button");
  assert.ok(button, "the loading action must exist");
  button.fire("click");
  await flushPromises();
  app.setEditorValue("src/main.cpp", "// saved source");
  app.ui.editor.value = "// learner edits after fallback";
  app.handleEditorInput();
  const amd = provideAmd(app);
  const editors = provideMonaco(app);
  lateLoad();
  for (const callback of amd.pending) callback.ready();
  assertFallback(app, result);
  assert.equal(editors.length, 0, "late script execution cannot create a replacement editor");
  assert.equal(app.ui.editor.value, "// learner edits after fallback");
  assert.equal(app.state.dirtyFiles.has("src/main.cpp"), true);
  const writes = [];
  app.setApi(async (url, options) => { writes.push([url, options.method, options.body.content]); return { saved: true }; });
  assert.equal(await app.saveFile("src/main.cpp", { silent: true }), true);
  assert.deepEqual(writes, [["/api/v1/workspaces/w1.hello-world/files/src/main.cpp", "PUT", "// learner edits after fallback"]]);
  app.setEditorValue("include/task.hpp", "// read-only reference");
  assert.equal(app.ui.editor.readOnly, true);
  app.handleEditorInput();
  assert.equal(await app.saveFile("include/task.hpp"), false);
  assert.equal(writes.length, 1, "fallback preserves the reference-file write guard");
});

test("late AMD success cannot replace source edited after timeout", async () => {
  const app = application();
  const amd = provideAmd(app);
  const editors = provideMonaco(app);
  const result = observe(app.ensureEditorReady());
  await app.timers.advance(8000);
  app.setEditorValue("src/main.cpp", "// learner edits after timeout");
  amd.pending[0].ready();
  amd.pending[0].failed();
  await flushPromises();
  assertFallback(app, result);
  assert.equal(editors.length, 0);
  assert.equal(app.ui.editor.value, "// learner edits after timeout");
  assert.equal(amd.pending.length, 1, "late failures must not start another CDN attempt");
});

test("successful Monaco initialization stays active after the deadline and stale events", async () => {
  const app = application();
  app.bindEvents();
  const result = observe(app.ensureEditorReady());
  const lateError = app.scripts[0].onerror;
  const amd = provideAmd(app);
  const editors = provideMonaco(app);
  app.scripts[0].onload();
  amd.pending[0].ready();
  await flushPromises();
  assert.deepEqual(result, { status: "resolved", value: true });
  app.setEditorValue("src/main.cpp", "// Monaco source");
  const sourceModel = app.state.models.get("src/main.cpp");
  app.setEditorValue("include/task.hpp", "// reference");
  assert.equal(editors[0].options.readOnly, true);
  app.setEditorValue("src/main.cpp", "// Monaco source");
  assert.equal(editors[0].getModel(), sourceModel, "normal file switches reuse Monaco models");
  assert.equal(editors[0].options.readOnly, false);
  lateError();
  amd.pending[0].ready();
  app.byId("editor-fallback-button")?.fire("click");
  await app.timers.advance(8000);
  assert.equal(app.state.editorInstance, editors[0], "settlement cannot replace a functioning editor");
  assert.equal(editors.length, 1);
  assert.equal(app.ui.editor.hidden, true);
  assert.equal(app.ui.monacoContainer.hidden, false);
  assert.equal(app.byId("editor-loading").hidden, true);
  assert.equal(await app.ensureEditorReady(), true);
});

test("immediate loader errors exhaust the CDNs and settle without waiting", async () => {
  const app = application();
  const result = observe(app.ensureEditorReady());
  for (let index = 0; index < 3; index += 1) app.scripts[index].onerror();
  await flushPromises();
  assert.equal(app.scripts.length, 3);
  assertFallback(app, result);
});

test("an immediate AMD error can recover through the next CDN", async () => {
  const app = application();
  const amd = provideAmd(app);
  const editors = provideMonaco(app);
  const result = observe(app.ensureEditorReady());
  amd.pending[0].failed();
  assert.equal(amd.pending.length, 2);
  assert.match(amd.roots[1], /^https:\/\/gcore\.jsdelivr\.net\//);
  amd.pending[1].ready();
  await flushPromises();
  assert.deepEqual(result, { status: "resolved", value: true });
  assert.equal(app.state.editorInstance, editors[0]);
});

test("a failed AMD attempt cannot later take over the active CDN attempt", async () => {
  const app = application();
  const amd = provideAmd(app);
  const editors = provideMonaco(app);
  const result = observe(app.ensureEditorReady());
  amd.pending[0].failed();
  amd.pending[0].ready();
  amd.pending[0].failed();
  await flushPromises();
  assert.equal(result.status, "pending", "only the active CDN attempt may finish loading");
  assert.equal(editors.length, 0);
  assert.equal(amd.pending.length, 2, "repeated callbacks cannot skip or duplicate CDN attempts");
  amd.pending[1].ready();
  await flushPromises();
  assert.deepEqual(result, { status: "resolved", value: true });
});
