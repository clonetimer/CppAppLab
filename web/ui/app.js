(() => {
  "use strict";

  const state = {
    csrfToken: "",
    tasks: [],
    selectedTaskId: "",
    task: null,
    files: [],
    activeFile: "",
    workspaceReady: false,
    dirty: false,
    busy: false
  };

  const $ = (id) => document.getElementById(id);
  const ui = {
    engine: $("engine-status"),
    progress: $("progress-summary"),
    taskCount: $("task-count"),
    search: $("task-search"),
    courseList: $("course-list"),
    welcome: $("welcome-panel"),
    workbench: $("workbench-panel"),
    continueButton: $("continue-button"),
    taskBreadcrumb: $("task-breadcrumb"),
    taskTitle: $("task-title"),
    taskState: $("task-state"),
    taskSubtitle: $("task-subtitle"),
    startButton: $("start-button"),
    lesson: $("lesson-content"),
    hint: $("hint-content"),
    fileList: $("file-list"),
    activeFile: $("active-file"),
    editor: $("source-editor"),
    saveButton: $("save-button"),
    checkButton: $("check-button"),
    saveState: $("save-state"),
    workspaceState: $("workspace-state"),
    console: $("console-output"),
    checkResult: $("check-result"),
    toast: $("toast")
  };

  function escapeHtml(value) {
    return String(value).replace(/[&<>"']/g, (character) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[character]);
  }

  function renderInline(value) {
    return escapeHtml(value)
      .replace(/\x60([^\x60]+)\x60/g, "<code>$1</code>")
      .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
      .replace(/\*([^*]+)\*/g, "<em>$1</em>");
  }

  function renderMarkdown(source) {
    const lines = String(source || "").replace(/\r\n/g, "\n").split("\n");
    const output = [];
    let paragraph = [];
    let listItems = [];
    let listType = "";
    let codeLines = null;
    let codeLanguage = "";

    const flushParagraph = () => {
      if (paragraph.length) {
        output.push("<p>" + paragraph.map(renderInline).join("<br>") + "</p>");
        paragraph = [];
      }
    };
    const flushList = () => {
      if (listItems.length) {
        const tag = listType === "ol" ? "ol" : "ul";
        output.push("<" + tag + ">" + listItems.map((item) => "<li>" + renderInline(item) + "</li>").join("") + "</" + tag + ">");
        listItems = [];
        listType = "";
      }
    };
    const flushText = () => { flushParagraph(); flushList(); };

    for (const line of lines) {
      const fence = line.match(/^\x60{3}([a-zA-Z0-9_+-]*)\s*$/);
      if (fence) {
        if (codeLines === null) {
          flushText();
          codeLines = [];
          codeLanguage = fence[1];
        } else {
          const languageClass = codeLanguage ? ' class="language-' + escapeHtml(codeLanguage) + '"' : "";
          output.push("<pre><code" + languageClass + ">" + escapeHtml(codeLines.join("\n")) + "</code></pre>");
          codeLines = null;
          codeLanguage = "";
        }
        continue;
      }
      if (codeLines !== null) {
        codeLines.push(line);
        continue;
      }
      if (!line.trim()) {
        flushText();
        continue;
      }
      const heading = line.match(/^(#{1,3})\s+(.+)$/);
      if (heading) {
        flushText();
        const level = heading[1].length;
        output.push("<h" + level + ">" + renderInline(heading[2]) + "</h" + level + ">");
        continue;
      }
      if (/^\s*([-*_])\1\1+\s*$/.test(line)) {
        flushText();
        output.push("<hr>");
        continue;
      }
      const quote = line.match(/^>\s?(.*)$/);
      if (quote) {
        flushText();
        output.push("<blockquote>" + renderInline(quote[1]) + "</blockquote>");
        continue;
      }
      const unordered = line.match(/^\s*[-*+]\s+(.+)$/);
      const ordered = line.match(/^\s*\d+[.)]\s+(.+)$/);
      if (unordered || ordered) {
        flushParagraph();
        const nextType = ordered ? "ol" : "ul";
        if (listType && listType !== nextType) flushList();
        listType = nextType;
        listItems.push((unordered || ordered)[1]);
        continue;
      }
      flushList();
      paragraph.push(line);
    }
    if (codeLines !== null) {
      output.push("<pre><code>" + escapeHtml(codeLines.join("\n")) + "</code></pre>");
    }
    flushText();
    return output.join("\n");
  }

  async function api(path, options = {}) {
    const method = (options.method || "GET").toUpperCase();
    const headers = new Headers(options.headers || {});
    if (options.body !== undefined) headers.set("Content-Type", "application/json");
    if (["POST", "PUT", "PATCH", "DELETE"].includes(method)) {
      headers.set("X-CppAppLab-Token", state.csrfToken);
    }
    const response = await fetch(path, {
      method,
      headers,
      body: options.body === undefined ? undefined : JSON.stringify(options.body),
      credentials: "same-origin",
      cache: "no-store"
    });
    const type = response.headers.get("content-type") || "";
    const payload = type.includes("application/json") ? await response.json() : await response.text();
    if (!response.ok) {
      const message = payload && payload.error ? payload.error : "Request failed (" + response.status + ")";
      throw new Error(message);
    }
    return payload;
  }

  let toastTimeout;
  function toast(message) {
    ui.toast.textContent = message;
    ui.toast.classList.add("visible");
    clearTimeout(toastTimeout);
    toastTimeout = setTimeout(() => ui.toast.classList.remove("visible"), 2800);
  }

  function stateLabel(value) {
    return ({
      "AVAILABLE": "可开始",
      "NOT STARTED": "未开始",
      "IN PROGRESS": "进行中",
      "PASSED": "已通过",
      "NEEDS WORK": "需要修改",
      "STALE": "结果已过期",
      "LOCKED": "未解锁",
      "UNKNOWN": "状态未知"
    })[value] || value || "未知";
  }

  function updateProgressSummary() {
    const passed = state.tasks.filter((task) => task.state === "PASSED").length;
    ui.progress.textContent = passed + " / " + state.tasks.length + " 个任务已通过";
    ui.taskCount.textContent = String(state.tasks.length);
  }

  function renderCourseList() {
    const query = ui.search.value.trim().toLocaleLowerCase();
    ui.courseList.replaceChildren();
    const weeks = new Map();
    for (const task of state.tasks) {
      const searchable = (task.id + " " + task.title + " W" + task.week).toLocaleLowerCase();
      if (query && !searchable.includes(query)) continue;
      if (!weeks.has(task.week)) weeks.set(task.week, []);
      weeks.get(task.week).push(task);
    }
    if (!weeks.size) {
      const empty = document.createElement("div");
      empty.className = "loading";
      empty.textContent = "没有找到匹配的课程。";
      ui.courseList.append(empty);
      return;
    }
    for (const [week, tasks] of weeks) {
      const group = document.createElement("div");
      group.className = "week-group";
      const heading = document.createElement("div");
      heading.className = "week-heading";
      heading.textContent = "W" + week + " · " + ({1:"C++ 基础",2:"STL 与文件",3:"面向对象",4:"工程质量",5:"Linux 系统",6:"并发编程",7:"网络编程",8:"HTTP 服务",9:"协议安全"}[week] || "进阶课程");
      group.append(heading);
      for (const task of tasks) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "task-link" + (task.id === state.selectedTaskId ? " active" : "");
        button.dataset.state = task.state;
        button.title = task.title;
        const marker = document.createElement("span");
        marker.className = "task-marker";
        marker.textContent = task.state === "PASSED" ? "✓" : task.state === "LOCKED" ? "·" : task.id.split(".").slice(-1)[0].slice(0, 2).toUpperCase();
        const copy = document.createElement("span");
        copy.className = "task-link-copy";
        const title = document.createElement("strong");
        title.textContent = task.title.replace(/^W\d+\s*[·:]\s*/, "");
        const subtitle = document.createElement("small");
        subtitle.textContent = stateLabel(task.state);
        copy.append(title, subtitle);
        button.append(marker, copy);
        button.addEventListener("click", () => selectTask(task.id));
        group.append(button);
      }
      ui.courseList.append(group);
    }
  }

  function updateTaskState(taskId, nextState) {
    const item = state.tasks.find((task) => task.id === taskId);
    if (item) item.state = nextState;
    if (state.task && state.task.id === taskId) {
      state.task.state = nextState;
      ui.taskState.textContent = stateLabel(nextState);
      ui.taskState.dataset.state = nextState;
    }
    updateProgressSummary();
    renderCourseList();
  }

  async function loadTasks() {
    const data = await api("/api/v1/tasks");
    state.tasks = data.tasks || [];
    updateProgressSummary();
    renderCourseList();
    const next = state.tasks.find((task) => task.state === "IN PROGRESS" || task.state === "NEEDS WORK" || task.state === "STALE")
      || state.tasks.find((task) => task.state === "AVAILABLE")
      || state.tasks.find((task) => task.state === "PASSED");
    ui.continueButton.disabled = !next;
    ui.continueButton.textContent = next ? "继续学习  →" : "课程已完成";
    if (next) ui.continueButton.onclick = () => selectTask(next.id);
  }

  async function selectTask(taskId) {
    if (state.dirty && !window.confirm("当前文件有未保存的修改，仍要切换课程吗？")) return;
    state.selectedTaskId = taskId;
    state.task = null;
    state.files = [];
    state.activeFile = "";
    state.workspaceReady = false;
    state.dirty = false;
    ui.welcome.hidden = true;
    ui.workbench.hidden = false;
    ui.lesson.innerHTML = '<div class="loading">正在读取课程讲解…</div>';
    ui.hint.hidden = true;
    ui.editor.value = "";
    ui.editor.disabled = true;
    ui.saveButton.disabled = true;
    ui.checkButton.disabled = true;
    ui.fileList.replaceChildren();
    ui.activeFile.textContent = "未选择文件";
    ui.workspaceState.textContent = "尚未创建";
    ui.console.textContent = "开始任务后，可以在这里查看运行与验收输出。";
    ui.checkResult.textContent = "等待执行";
    renderCourseList();
    try {
      const task = await api("/api/v1/tasks/" + encodeURIComponent(taskId));
      state.task = task;
      ui.taskBreadcrumb.textContent = task.id;
      ui.taskTitle.textContent = task.title;
      ui.taskSubtitle.textContent = "W" + task.week + " · " + task.source_files.length + " 个源文件 · " +
        (task.acceptance_kind === "exact_stdout" ? "精确输出验收" : "固定 GoogleTest 验收");
      ui.taskState.textContent = stateLabel(task.state);
      ui.taskState.dataset.state = task.state;
      ui.lesson.innerHTML = renderMarkdown(task.lesson_markdown);
      const locked = task.state === "LOCKED";
      ui.startButton.disabled = locked;
      ui.startButton.textContent = locked ? "完成前置任务后解锁" :
        task.state === "PASSED" ? "打开学习工作区  →" : "开始 / 继续学习  →";
      if (["IN PROGRESS", "NEEDS WORK", "STALE", "PASSED"].includes(task.state)) {
        await loadWorkspaceFiles();
      }
    } catch (error) {
      ui.lesson.textContent = error.message;
      toast(error.message);
    }
  }

  async function startTask() {
    if (!state.task || state.busy) return;
    setBusy(true, "正在准备学习工作区…");
    try {
      const result = await api("/api/v1/tasks/" + encodeURIComponent(state.task.id) + "/start", {
        method: "POST", body: {}
      });
      state.workspaceReady = true;
      ui.workspaceState.textContent = result.resumed ? "已恢复" : "已创建";
      await loadWorkspaceFiles();
      toast(result.resumed ? "已恢复上次的学习工作区。" : "学习工作区已创建。");
    } catch (error) {
      toast(error.message);
      ui.console.textContent = error.message;
    } finally {
      setBusy(false);
    }
  }

  async function loadWorkspaceFiles() {
    if (!state.task) return;
    const data = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files");
    state.files = data.files || [];
    state.workspaceReady = true;
    ui.workspaceState.textContent = "工作区已就绪";
    ui.fileList.replaceChildren();
    if (!state.files.length) {
      const empty = document.createElement("span");
      empty.className = "empty-files";
      empty.textContent = "当前任务没有可编辑的源文件";
      ui.fileList.append(empty);
      return;
    }
    for (const file of state.files) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "file-tab" + (file.path === state.activeFile ? " active" : "");
      button.textContent = file.path;
      button.title = file.path;
      button.addEventListener("click", () => loadFile(file.path));
      ui.fileList.append(button);
    }
    if (!state.activeFile || !state.files.some((file) => file.path === state.activeFile)) {
      await loadFile(state.files[0].path);
    } else {
      renderFileTabs();
    }
    ui.saveButton.disabled = false;
    ui.checkButton.disabled = false;
  }

  function renderFileTabs() {
    for (const button of ui.fileList.querySelectorAll(".file-tab")) {
      button.classList.toggle("active", button.textContent === state.activeFile);
    }
  }

  async function loadFile(path) {
    if (state.dirty && !window.confirm("当前文件有未保存的修改，仍要切换文件吗？")) return;
    if (!state.task) return;
    const encodedPath = path.split("/").map(encodeURIComponent).join("/");
    try {
      const data = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files/" + encodedPath);
      state.activeFile = path;
      ui.editor.value = data.content;
      ui.editor.disabled = false;
      state.dirty = false;
      ui.activeFile.textContent = path;
      ui.saveState.textContent = "已加载";
      ui.saveButton.disabled = false;
      ui.checkButton.disabled = false;
      renderFileTabs();
    } catch (error) {
      toast(error.message);
    }
  }

  async function saveFile() {
    if (!state.task || !state.activeFile || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存…");
    try {
      const encodedPath = state.activeFile.split("/").map(encodeURIComponent).join("/");
      await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files/" + encodedPath, {
        method: "PUT", body: { content: ui.editor.value }
      });
      state.dirty = false;
      ui.saveState.textContent = "已保存到本地工作区";
      toast("代码已保存。");
      updateTaskState(state.task.id, state.task.state === "PASSED" ? "STALE" : state.task.state);
    } catch (error) {
      toast(error.message);
      ui.saveState.textContent = "保存失败";
    } finally {
      setBusy(false);
    }
  }

  async function runCheck() {
    if (!state.task || !state.workspaceReady || state.busy) return;
    if (state.dirty) await saveFile();
    if (state.dirty) return;
    setBusy(true, "正在执行固定验收…");
    ui.checkResult.textContent = "正在测试";
    ui.console.textContent = "正在调用本地 GCC / CMake / GoogleTest 验收流程…";
    try {
      const result = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/check", {
        method: "POST", body: {}
      });
      ui.console.textContent = result.output || "(没有输出)";
      ui.checkResult.textContent = result.passed ? "PASS" : "未通过";
      ui.checkResult.className = "check-result " + (result.passed ? "pass" : "fail");
      updateTaskState(state.task.id, result.passed ? "PASSED" : "NEEDS WORK");
      toast(result.passed ? "恭喜，固定验收已通过。" : "验收未通过，请查看诊断输出并继续修改。");
    } catch (error) {
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "执行失败";
      ui.checkResult.className = "check-result fail";
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function showHint(level) {
    if (!state.task) return;
    try {
      const data = await api("/api/v1/tasks/" + encodeURIComponent(state.task.id) + "/hints/" + level);
      ui.hint.innerHTML = "<strong>渐进式提示 " + level + "</strong>" + renderMarkdown(data.markdown);
      ui.hint.hidden = false;
      ui.hint.scrollIntoView({ behavior: "smooth", block: "nearest" });
    } catch (error) {
      toast(error.message);
    }
  }

  function setBusy(value, saveLabel) {
    state.busy = value;
    ui.startButton.disabled = value || !state.task || state.task.state === "LOCKED";
    ui.saveButton.disabled = value || !state.workspaceReady || !state.activeFile;
    ui.checkButton.disabled = value || !state.workspaceReady;
    if (saveLabel) ui.saveState.textContent = saveLabel;
  }

  function bindEvents() {
    ui.search.addEventListener("input", renderCourseList);
    ui.startButton.addEventListener("click", startTask);
    ui.saveButton.addEventListener("click", saveFile);
    ui.checkButton.addEventListener("click", runCheck);
    ui.continueButton.addEventListener("click", () => {
      const next = state.tasks.find((task) => task.state === "IN PROGRESS" || task.state === "NEEDS WORK" || task.state === "STALE")
        || state.tasks.find((task) => task.state === "AVAILABLE");
      if (next) selectTask(next.id);
    });
    ui.editor.addEventListener("input", () => {
      state.dirty = true;
      ui.saveState.textContent = "有未保存的修改";
    });
    ui.editor.addEventListener("keydown", (event) => {
      if (event.key === "Tab") {
        event.preventDefault();
        const start = ui.editor.selectionStart;
        const end = ui.editor.selectionEnd;
        ui.editor.setRangeText("    ", start, end, "end");
        state.dirty = true;
        ui.saveState.textContent = "有未保存的修改";
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        saveFile();
      }
    });
    document.querySelectorAll("[data-hint]").forEach((button) => {
      button.addEventListener("click", () => showHint(Number(button.dataset.hint)));
    });
    window.addEventListener("beforeunload", (event) => {
      if (state.dirty) {
        event.preventDefault();
        event.returnValue = "";
      }
    });
  }

  async function init() {
    bindEvents();
    try {
      const session = await api("/api/v1/session");
      state.csrfToken = session.csrf_token;
      const health = await api("/api/v1/health");
      if (health.status !== "ok") throw new Error("本地 CppAppLab 引擎未就绪。");
      ui.engine.classList.add("online");
      ui.engine.innerHTML = '<span class="pulse"></span>本地引擎已连接';
      await loadTasks();
      if (state.tasks.length) {
        const next = state.tasks.find((task) => ["IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state))
          || state.tasks.find((task) => task.state === "AVAILABLE");
        if (next) {
          ui.continueButton.disabled = false;
          ui.continueButton.textContent = "继续学习  →";
          ui.continueButton.onclick = () => selectTask(next.id);
        }
      }
    } catch (error) {
      ui.engine.textContent = "本地引擎未连接";
      ui.engine.classList.remove("online");
      ui.courseList.innerHTML = '<div class="loading">无法连接本地服务。请从 CppAppLab 启动网页工作台。</div>';
      ui.continueButton.textContent = "服务未就绪";
      toast(error.message);
    }
  }

  init();
})();
