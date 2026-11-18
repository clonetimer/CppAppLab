"use strict";

// Load the real renderer and application action unchanged. Only browser/network
// boundaries are adapted here; Chromium coverage owns scrolling and layout.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const test = require("node:test");

const root = path.join(__dirname, "..");

class Element {
  constructor(tag = "div") {
    this.tagName = tag.toUpperCase();
    this.children = [];
    this.dataset = {};
    this.textContent = "";
    this.value = "";
    this.classList = { add() {}, remove() {}, toggle() {} };
  }
  append(...items) { this.children.push(...items); }
  replaceChildren(...items) { this.children = items; }
  addEventListener() {}
  setAttribute() {}
  querySelectorAll(selector) {
    const children = this.children.flatMap((child) => [child, ...child.querySelectorAll("*")]);
    if (selector === "[data-field]") return children.filter((child) => child.dataset.field);
    if (selector === "input, select, textarea") {
      return children.filter((child) => ["INPUT", "SELECT", "TEXTAREA"].includes(child.tagName));
    }
    return selector === "*" ? children : [];
  }
}

function application() {
  const elements = new Map();
  const document = {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, new Element());
      return elements.get(id);
    },
    createElement: (tag) => new Element(tag), querySelectorAll: () => []
  };
  const context = vm.createContext({ document, console, Map, Set, URL, Headers,
    setTimeout: () => 1, clearTimeout() {}, window: { addEventListener() {} } });
  const filename = path.join(root, "web", "ui", "app.js");
  const source = fs.readFileSync(filename, "utf8");
  const injected = source.replace(/\n  init\(\);\n\}\)\(\);\s*$/, `
  globalThis.workbench = { state, ui, renderMarkdown, runApplication, updateApplicationGuidance,
    setApi(handler) { api = handler; } };
})();`);
  assert.notEqual(injected, source, "test adapter must replace only automatic startup");
  vm.runInContext(injected, context, { filename });
  return context.workbench;
}

for (const lesson of [
  { file: "w2/todo.md", headers: ["Command", "Behavior"], rows: 4,
    firstCell: "<code>todo add &lt;title&gt;</code>", lastCell: "Remove an existing task" },
  { file: "w8/connection-policy.md", headers: ["Request version", "Connection options", "Result"], rows: 6,
    firstCell: "HTTP/1.1", lastCell: "Close" },
  { file: "w8/observability.md", headers: ["Metric", "Semantics"], rows: 18,
    firstCell: "<code>accepted_connections</code>", lastCell: "User log callbacks that threw" }
]) {
  test("released " + lesson.file + " renders its instructional table with semantic cells", () => {
    const source = fs.readFileSync(path.join(root, "course", "lessons", lesson.file), "utf8");
    const html = application().renderMarkdown(source);
    const table = html.match(/<table>[\s\S]*?<\/table>/g) || [];
    assert.equal(table.length, 1, "the released lesson must expose one real table, not raw pipe paragraphs");
    assert.match(table[0], /<thead><tr>/);
    for (const header of lesson.headers) assert.ok(table[0].includes('<th scope="col">' + header + "</th>"));
    const body = table[0].match(/<tbody>([\s\S]*)<\/tbody>/)[1];
    assert.equal((body.match(/<tr>/g) || []).length, lesson.rows);
    assert.equal((body.match(/<td>/g) || []).length, lesson.rows * lesson.headers.length);
    assert.ok(body.startsWith("<tr><td>" + lesson.firstCell + "</td>"));
    assert.ok(body.endsWith("<td>" + lesson.lastCell + "</td></tr>"));
    assert.doesNotMatch(html, /<p>\|(?:Command|Request version|Metric)\s*\|/);
    assert.match(html, /<h2>Acceptance<\/h2>/, "table parsing must leave the rest of the lesson intact");
  });
}

test("tables expose a labelled keyboard-focusable scrolling region", () => {
  const html = application().renderMarkdown("| Key | Meaning |\n| --- | --- |\n| one | first |");
  const wrapper = html.match(/<div\b[^>]*>/)?.[0] || "";
  assert.match(wrapper, /class="markdown-table-scroll"/);
  assert.match(wrapper, /role="region"/);
  assert.match(wrapper, /tabindex="0"/);
  assert.match(wrapper, /aria-label="[^"<>]+"/);
  assert.ok(html.endsWith("</table></div>"));
});

test("escaped pipes and pipes inside inline code stay in their table cells", () => {
  const html = application().renderMarkdown([
    "Before the table.", "",
    "Expression | Meaning", "--- | ---",
    "`left | right` | choose **either** value",
    "literal \\| pipe | keep *one* cell", "",
    "After the table."
  ].join("\n"));
  assert.match(html, /<td><code>left \| right<\/code><\/td><td>choose <strong>either<\/strong> value<\/td>/);
  assert.match(html, /<td>literal \| pipe<\/td><td>keep <em>one<\/em> cell<\/td>/);
  assert.equal((html.match(/<td>/g) || []).length, 4);
  assert.ok(html.startsWith("<p>Before the table.</p>\n"));
  assert.ok(html.endsWith("\n<p>After the table.</p>"));
});

test("HTML-like table input remains escaped while exact file code survives", () => {
  const html = application().renderMarkdown([
    '| <script>alert("header")</script> | Reference |', '|---|---|',
    '| <img src=x onerror="alert(1)"> & text | `src/calculator.cpp` |'
  ].join("\n"));
  assert.match(html, /<th scope="col">&lt;script&gt;alert\(&quot;header&quot;\)&lt;\/script&gt;<\/th>/);
  assert.match(html, /<td>&lt;img src=x onerror=&quot;alert\(1\)&quot;&gt; &amp; text<\/td>/);
  assert.match(html, /<td><code>src\/calculator\.cpp<\/code><\/td>/);
  assert.doesNotMatch(html, /<script|<img|onclick=/);
});

test("table splitting preserves code backslashes and treats unmatched backticks as literal text", () => {
  const html = application().renderMarkdown([
    "| Expression | Meaning |", "|---|---|",
    "| `path\\\\name | value` | literal |",
    "| unmatched `left | right |"
  ].join("\n"));
  assert.ok(html.includes("<td><code>path\\\\name | value</code></td><td>literal</td>"));
  assert.ok(html.includes("<td>unmatched `left</td><td>right</td>"));
  assert.equal((html.match(/<td>/g) || []).length, 4);
});

test("ordinary prose pipes, malformed separators and fenced tables remain text", () => {
  const render = application().renderMarkdown;
  for (const source of [
    "Use left | right for a bitwise operation.",
    "Name | Meaning\n--- | prose\nleft | right",
    "Name | Meaning\n--- | --- | ---\nleft | right",
    "`left | right`\n--- | ---"
  ]) {
    assert.doesNotMatch(render(source), /<table>/);
    assert.match(render(source), /\|/);
  }
  const fenced = render("```text\n| A | B |\n|---|---|\n| 1 | 2 |\n```\n\nAfter.");
  assert.match(fenced, /<pre><code class="language-text">\| A \| B \|\n\|---\|---\|\n\| 1 \| 2 \|<\/code><\/pre>/);
  assert.doesNotMatch(fenced, /<table>/);
  assert.ok(fenced.endsWith("<p>After.</p>"));
});

test("table parsing ends before unrelated pipe prose without dropping content", () => {
  const html = application().renderMarkdown("| A | B |\n|---|---|\n| one | two |\nleft | middle | right\n## Next");
  assert.equal((html.match(/<td>/g) || []).length, 2);
  assert.match(html, /<\/table><\/div>\n<p>left \| middle \| right<\/p>\n<h2>Next<\/h2>/);
});

function calculatorApplication() {
  const app = application();
  app.state.task = {
    id: "w1.calculator", title: "W1 · Multi-file Calculator", state: "PASSED", prerequisites: ["w1.hello-world"],
    week: 1, order: 2, source_files: ["src/main.cpp", "src/calculator.cpp"], acceptance_kind: "cmake_ctest",
    state_reason: "", service_profile: null,
    runtime_profile: { label: "命令行计算器", actions: [{ id: "calculate", label: "执行计算", fields: [
      { name: "operation", label: "运算", type: "select", options: ["add", "subtract", "multiply", "divide"] },
      { name: "left", label: "左操作数", type: "number" }, { name: "right", label: "右操作数", type: "number" }
    ] }] }
  };
  app.state.tasks = [app.state.task];
  app.state.selectedTaskId = "w1.calculator";
  app.state.workspaceReady = true;
  app.state.progressFresh = true;
  app.ui.appAction.value = "calculate";
  for (const [field, value] of [["operation", "divide"], ["left", "1"], ["right", "0"]]) {
    const control = new Element("input");
    control.dataset.field = field;
    control.dataset.fieldType = field === "operation" ? "select" : "number";
    control.value = value;
    app.ui.appFields.append(control);
  }
  return app;
}

test("calculator negative completion preserves diagnostics and formal history without declaring incorrect code", async () => {
  const app = calculatorApplication();
  const accepted = { taskId: "w1.calculator", action: "check", passed: true };
  app.state.lastAction = accepted;
  const record = new Element();
  record.textContent = "Existing formal PASS";
  app.ui.historyList.append(record);
  const requests = [];
  app.setApi(async (url, options) => {
    requests.push([url, options.method, options.body.action, Array.from(options.body.values)]);
    return { task_id: "w1.calculator", action: "calculate", exit_code: 1, success: false,
      records_evidence: false, output: "error: division by zero\n" };
  });
  app.updateApplicationGuidance();
  await app.runApplication();
  assert.deepEqual(requests, [["/api/v1/workspaces/w1.calculator/app-run", "POST", "calculate", ["divide", "1", "0"]]]);
  assert.equal(app.ui.console.textContent, "error: division by zero\n");
  assert.match(app.ui.checkResult.className, /\bfail\b/);
  assert.doesNotMatch(app.ui.checkResult.className, /\bpass\b/);
  assert.match(app.ui.checkResult.textContent, /非零/);
  assert.doesNotMatch(app.ui.checkResult.textContent, /成功|失败|退出码\s*1/);
  assert.match(app.ui.toast.textContent, /课程.*预期|教材.*预期/);
  assert.match(app.ui.toast.textContent, /没有写入正式验收记录/);
  assert.doesNotMatch(app.ui.toast.textContent, /修改代码后重试/);
  assert.match(app.ui.appRunnerHelp.textContent, /divide 1 0/);
  assert.match(app.ui.appRunnerHelp.textContent, /error: division by zero/);
  assert.match(app.ui.appRunnerHelp.textContent, /退出码\s*3/);
  assert.equal(app.state.task.state, "PASSED");
  assert.equal(app.state.lastAction, accepted);
  assert.deepEqual(app.ui.historyList.children, [record]);
  assert.equal(app.state.busy, false);
});

test("successful application completion still reports success without recording formal evidence", async () => {
  const app = calculatorApplication();
  const values = ["add", "2", "3"];
  app.ui.appFields.querySelectorAll("[data-field]").forEach((control, index) => { control.value = values[index]; });
  app.setApi(async () => ({ task_id: "w1.calculator", action: "calculate", exit_code: 0,
    success: true, records_evidence: false, output: "5\n" }));
  await app.runApplication();
  assert.equal(app.ui.console.textContent, "5\n");
  assert.match(app.ui.checkResult.className, /\bpass\b/);
  assert.match(app.ui.checkResult.textContent, /成功/);
  assert.match(app.ui.toast.textContent, /没有写入正式验收记录/);
  assert.equal(app.ui.historyList.children.length, 0);
});

test("application request errors remain failures with their diagnostic output", async () => {
  const app = calculatorApplication();
  app.setApi(async () => { throw new Error("Application binary is unavailable."); });
  await app.runApplication();
  assert.equal(app.ui.console.textContent, "Application binary is unavailable.");
  assert.match(app.ui.checkResult.className, /\bfail\b/);
  assert.match(app.ui.checkResult.textContent, /失败/);
  assert.equal(app.state.task.state, "PASSED");
  assert.equal(app.state.busy, false);
});
