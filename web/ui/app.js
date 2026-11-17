(() => {
  "use strict";

  const MONACO_VERSION = "0.52.2";
  const MONACO_ROOTS = [
    "https://cdn.jsdelivr.net/npm/monaco-editor@" + MONACO_VERSION + "/min",
    "https://gcore.jsdelivr.net/npm/monaco-editor@" + MONACO_VERSION + "/min",
    "https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/" + MONACO_VERSION + "/min"
  ];
  const state = {
    csrfToken: "",
    tasks: [],
    selectedTaskId: "",
    task: null,
    files: [],
    activeFile: "",
    workspaceReady: false,
    dirtyFiles: new Set(),
    fileContents: new Map(),
    fileVersions: new Map(),
    models: new Map(),
    monaco: null,
    editorInstance: null,
    editorReady: null,
    busy: false,
    autosaveTimer: null,
    serviceSnapshot: null,
    serviceClients: [],
    serviceMessages: []
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
    historyButton: $("history-button"),
    historyPanel: $("history-panel"),
    historyClose: $("history-close"),
    historySummary: $("history-summary"),
    historyList: $("history-list"),
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
    monacoContainer: $("monaco-editor"),
    editorEngineStatus: $("editor-engine-status"),
    buildButton: $("build-button"),
    runButton: $("run-button"),
    appRunner: $("app-runner"),
    appProfileLabel: $("app-profile-label"),
    appRunnerHelp: $("app-runner-help"),
    appAction: $("app-action"),
    appFields: $("app-fields"),
    appBuildButton: $("app-build-button"),
    appExecuteButton: $("app-execute-button"),
    serviceRunner: $("service-runner"),
    serviceProfileLabel: $("service-profile-label"),
    serviceRunnerHelp: $("service-runner-help"),
    serviceControls: $("service-controls"),
    serviceState: $("service-state"),
    servicePort: $("service-port"),
    serviceStartButton: $("service-start-button"),
    serviceRefreshButton: $("service-refresh-button"),
    serviceStopButton: $("service-stop-button"),
    serviceLogs: $("service-logs"),
    saveButton: $("save-button"),
    checkButton: $("check-button"),
    saveState: $("save-state"),
    workspaceState: $("workspace-state"),
    consoleTitle: $("console-title"),
    console: $("console-output"),
    diagnostics: $("diagnostics-list"),
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
        button.dataset.taskId = task.id;
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

  async function loadTaskHistory(taskId) {
    ui.historyPanel.hidden = false;
    ui.historySummary.textContent = "正在读取验收记录…";
    ui.historyList.replaceChildren();
    try {
      const data = await api("/api/v1/tasks/" + encodeURIComponent(taskId) + "/history");
      ui.historySummary.textContent = data.count
        ? "共 " + data.count + " 次正式验收 · 当前状态：" + stateLabel(data.current_state)
        : "尚无正式验收记录 · 当前状态：" + stateLabel(data.current_state);
      if (!data.records.length) {
        const empty = document.createElement("p");
        empty.className = "history-empty";
        empty.textContent = "完成一次正式验收后，记录会显示在这里。普通构建、运行和网络服务演示不会生成验收记录。";
        ui.historyList.append(empty);
        return;
      }
      for (const record of data.records.slice().reverse()) {
        const item = document.createElement("article");
        item.className = "history-record";
        const top = document.createElement("div");
        top.className = "history-record-top";
        const result = document.createElement("strong");
        result.className = record.result === "PASS" ? "history-pass" : "history-fail";
        result.textContent = record.result === "PASS" ? "通过" : "未通过";
        const timestamp = document.createElement("time");
        timestamp.textContent = record.timestamp;
        top.append(result, timestamp);
        const tests = document.createElement("p");
        tests.textContent = "测试结果：" + record.tests;
        const workspace = document.createElement("p");
        workspace.textContent = "工作区：" + (record.workspace || "未知");
        const fingerprint = document.createElement("code");
        fingerprint.textContent = "源码指纹：" + (record.fingerprint || "未知");
        item.append(top, tests, workspace, fingerprint);
        ui.historyList.append(item);
      }
    } catch (error) {
      ui.historySummary.textContent = "无法读取验收历史：" + error.message;
    }
  }

  async function loadTasks() {
    const data = await api("/api/v1/tasks");
    state.tasks = data.tasks || [];
    updateProgressSummary();
    renderCourseList();
    const next = state.tasks.find((task) => ["IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state))
      || state.tasks.find((task) => task.state === "AVAILABLE");
    ui.continueButton.disabled = !next;
    ui.continueButton.textContent = next ? "继续学习  →" : "课程已完成";
  }

  function disposeEditorModels() {
    if (state.editorInstance) {
      state.editorInstance.setModel(null);
    }
    for (const model of state.models.values()) model.dispose();
    state.models.clear();
    state.fileContents.clear();
    state.fileVersions.clear();
    state.dirtyFiles.clear();
    state.activeFile = "";
  }

  function fallbackToTextarea() {
    ui.monacoContainer.hidden = true;
    ui.editor.hidden = false;
    ui.editorEngineStatus.textContent = "基础编辑器（离线）";
    state.monaco = null;
    state.editorInstance = null;
  }

  async function ensureEditorReady() {
    if (state.editorReady) return state.editorReady;
    state.editorReady = new Promise((resolve) => {
      const configureAndLoad = (root, next) => {
        try {
          window.MonacoEnvironment = {
            getWorkerUrl: () => {
              const workerCode =
                "self.MonacoEnvironment = { baseUrl: '" + root + "/' }; " +
                "importScripts('" + root + "/vs/base/worker/workerMain.js');";
              return "data:text/javascript;charset=utf-8," + encodeURIComponent(workerCode);
            }
          };
          window.require.config({ paths: { vs: root + "/vs" } });
          window.require(["vs/editor/editor.main"], () => {
            try {
              state.monaco = window.monaco;
              state.editorInstance = state.monaco.editor.create(ui.monacoContainer, {
                value: "",
                language: "cpp",
                theme: "vs-dark",
                automaticLayout: true,
                minimap: { enabled: false },
                fontSize: 14,
                tabSize: 4,
                insertSpaces: true,
                scrollBeyondLastLine: false,
                renderLineHighlight: "all",
                smoothScrolling: true,
                glyphMargin: true,
                wordWrap: "off",
                padding: { top: 12, bottom: 12 }
              });
              state.editorInstance.onDidChangeModelContent(handleEditorInput);
              state.editorInstance.addCommand(
                state.monaco.KeyMod.CtrlCmd | state.monaco.KeyCode.KeyS,
                () => saveAllDirtyFiles()
              );
              ui.monacoContainer.hidden = false;
              ui.editor.hidden = true;
              ui.editorEngineStatus.textContent = "Monaco " + MONACO_VERSION;
              requestAnimationFrame(() => state.editorInstance && state.editorInstance.layout());
              resolve(true);
            } catch (error) {
              console.warn("Monaco editor initialization failed:", error);
              fallbackToTextarea();
              resolve(false);
            }
          }, () => next());
        } catch (error) {
          console.warn("Monaco CDN configuration failed:", error);
          next();
        }
      };

      const tryLoad = (index) => {
        if (index >= MONACO_ROOTS.length) {
          fallbackToTextarea();
          resolve(false);
          return;
        }
        const root = MONACO_ROOTS[index];
        const next = () => tryLoad(index + 1);
        if (window.require && typeof window.require.config === "function") {
          configureAndLoad(root, next);
          return;
        }
        const loader = document.createElement("script");
        loader.src = root + "/vs/loader.js";
        loader.crossOrigin = "anonymous";
        loader.onload = () => configureAndLoad(root, next);
        loader.onerror = next;
        document.head.append(loader);
      };

      tryLoad(0);
    });
    return state.editorReady;
  }

  function languageForPath(path) {
    const extension = path.split(".").pop().toLowerCase();
    if (["cpp", "cc", "cxx", "hpp", "hh", "hxx", "h"].includes(extension)) return "cpp";
    if (extension === "c") return "c";
    if (extension === "json") return "json";
    if (extension === "md") return "markdown";
    return "plaintext";
  }

  function modelUri(path) {
    const safePath = path.split("/").map((part) => encodeURIComponent(part)).join("/");
    return state.monaco.Uri.parse("inmemory://cppapplab/" + encodeURIComponent(state.task.id) + "/" + safePath);
  }

  function editorValueFor(path) {
    if (state.editorInstance && path === state.activeFile && state.editorInstance.getModel()) {
      return state.editorInstance.getValue();
    }
    if (!state.editorInstance && path === state.activeFile && !ui.editor.hidden) {
      return ui.editor.value;
    }
    const model = state.models.get(path);
    if (model) return model.getValue();
    return state.fileContents.get(path) || "";
  }

  function setEditorValue(path, value) {
    state.fileContents.set(path, value);
    if (state.editorInstance && state.monaco) {
      let model = state.models.get(path);
      if (!model) {
        model = state.monaco.editor.createModel(value, languageForPath(path), modelUri(path));
        state.models.set(path, model);
      } else if (!state.dirtyFiles.has(path)) {
        model.setValue(value);
      }
      state.editorInstance.setModel(model);
      state.editorInstance.updateOptions({ readOnly: false });
    } else {
      ui.editor.value = value;
      ui.editor.disabled = false;
    }
    state.activeFile = path;
    ui.activeFile.textContent = path;
  }

  function renderFileTabs() {
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
      button.title = file.path;
      const dirty = document.createElement("span");
      dirty.className = "file-dirty-dot";
      dirty.textContent = state.dirtyFiles.has(file.path) ? "● " : "";
      const name = document.createElement("span");
      name.textContent = file.path;
      button.append(dirty, name);
      button.addEventListener("click", () => loadFile(file.path));
      ui.fileList.append(button);
    }
  }

  function updateDirtyState() {
    const dirty = state.dirtyFiles.size > 0;
    ui.saveState.textContent = dirty ? "有未保存的修改" : "已保存到本地工作区";
    ui.saveButton.disabled = !dirty || state.busy || !state.workspaceReady;
    renderFileTabs();
  }

  function clearDiagnosticsForActiveModel() {
    if (state.monaco) {
      for (const model of state.models.values()) {
        state.monaco.editor.setModelMarkers(model, "cppapplab", []);
      }
    }
    ui.diagnostics.replaceChildren();
    ui.diagnostics.hidden = true;
  }

  function handleEditorInput() {
    if (!state.activeFile) return;
    state.dirtyFiles.add(state.activeFile);
    state.fileVersions.set(state.activeFile, (state.fileVersions.get(state.activeFile) || 0) + 1);
    state.fileContents.set(state.activeFile, editorValueFor(state.activeFile));
    clearDiagnosticsForActiveModel();
    updateDirtyState();
    scheduleAutosave();
  }

  function scheduleAutosave() {
    clearTimeout(state.autosaveTimer);
    ui.saveState.textContent = "自动保存将在片刻后执行…";
    state.autosaveTimer = setTimeout(() => {
      saveAllDirtyFiles({ silent: true }).catch((error) => toast(error.message));
    }, 900);
  }

  async function saveFile(path, options = {}) {
    if (!state.task || !path || !state.workspaceReady) return false;
    const content = editorValueFor(path);
    const savedVersion = state.fileVersions.get(path) || 0;
    const encodedPath = path.split("/").map(encodeURIComponent).join("/");
    try {
      await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files/" + encodedPath, {
        method: "PUT",
        body: { content }
      });
      if ((state.fileVersions.get(path) || 0) === savedVersion) {
        state.fileContents.set(path, content);
        state.dirtyFiles.delete(path);
      } else {
        state.fileContents.set(path, editorValueFor(path));
        scheduleAutosave();
      }
      if (state.task.state === "PASSED") updateTaskState(state.task.id, "STALE");
      updateDirtyState();
      if (!options.silent) toast("代码已保存。");
      return true;
    } catch (error) {
      ui.saveState.textContent = "保存失败";
      if (!options.silent) toast(error.message);
      return false;
    }
  }

  async function saveAllDirtyFiles(options = {}) {
    clearTimeout(state.autosaveTimer);
    let savedCount = 0;
    for (let pass = 0; state.dirtyFiles.size && pass < 5; pass += 1) {
      const paths = Array.from(state.dirtyFiles);
      for (const path of paths) {
        const saved = await saveFile(path, { silent: true });
        if (!saved) {
          if (!options.silent) toast("部分文件保存失败，请检查网络或工作区状态。");
          return false;
        }
        savedCount += 1;
      }
    }
    if (state.dirtyFiles.size) {
      scheduleAutosave();
      if (!options.silent) toast("代码仍在修改中，请稍后再运行。");
      return false;
    }
    if (!options.silent && savedCount) toast("所有修改均已保存。");
    updateDirtyState();
    return true;
  }

  async function loadFile(path) {
    if (!state.task) return;
    if (state.activeFile && state.dirtyFiles.has(state.activeFile)) {
      const saved = await saveFile(state.activeFile, { silent: true });
      if (!saved) return;
    }
    await ensureEditorReady();
    let content = state.fileContents.get(path);
    if (content === undefined) {
      const encodedPath = path.split("/").map(encodeURIComponent).join("/");
      try {
        const data = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files/" + encodedPath);
        content = data.content;
        state.fileContents.set(path, content);
      } catch (error) {
        toast(error.message);
        return;
      }
    }
    setEditorValue(path, content);
    ui.saveButton.disabled = false;
    ui.buildButton.disabled = false;
    ui.runButton.disabled = false;
    ui.checkButton.disabled = false;
    updateDirtyState();
    renderFileTabs();
    if (state.editorInstance) state.editorInstance.focus();
    else ui.editor.focus();
  }

  async function loadWorkspaceFiles() {
    if (!state.task) return;
    const data = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files");
    state.files = data.files || [];
    state.workspaceReady = true;
    ui.workspaceState.textContent = "工作区已就绪";
    updateApplicationGuidance();
    if (state.task && state.task.service_profile) {
      updateServiceSnapshot(state.serviceSnapshot || { state: "STOPPED", port: null, logs: "" });
    }
    const hasApplicationProfile = Boolean(state.task && state.task.runtime_profile);
    ui.appBuildButton.disabled = !hasApplicationProfile || state.busy;
    ui.appExecuteButton.disabled = !hasApplicationProfile || state.busy;
    renderFileTabs();
    if (!state.files.length) {
      ui.editor.disabled = true;
      ui.saveButton.disabled = true;
      ui.buildButton.disabled = true;
      ui.runButton.disabled = true;
      ui.checkButton.disabled = true;
      return;
    }
    if (!state.activeFile || !state.files.some((file) => file.path === state.activeFile)) {
      await loadFile(state.files[0].path);
    } else {
      await loadFile(state.activeFile);
    }
    await refreshServiceStatus(true);
  }

  function clearTaskEditorState() {
    if (state.editorInstance) state.editorInstance.setModel(null);
    for (const model of state.models.values()) model.dispose();
    state.models.clear();
    state.fileContents.clear();
    state.fileVersions.clear();
    state.dirtyFiles.clear();
    state.files = [];
    state.activeFile = "";
    state.workspaceReady = false;
    clearTimeout(state.autosaveTimer);
    ui.fileList.replaceChildren();
    ui.activeFile.textContent = "未选择文件";
    ui.editor.value = "";
    ui.editor.disabled = true;
    ui.saveButton.disabled = true;
    ui.buildButton.disabled = true;
    ui.runButton.disabled = true;
    ui.appBuildButton.disabled = true;
    ui.appExecuteButton.disabled = true;
    ui.appRunner.hidden = true;
    ui.appAction.replaceChildren();
    ui.appFields.replaceChildren();
    ui.serviceRunner.hidden = true;
    ui.serviceControls.replaceChildren();
    ui.serviceState.textContent = "未启动";
    ui.serviceState.dataset.state = "STOPPED";
    state.serviceClients = [];
    state.serviceMessages = [];
    ui.servicePort.textContent = "";
    ui.serviceLogs.textContent = "尚未启动网络服务。";
    ui.serviceStartButton.disabled = true;
    ui.serviceRefreshButton.disabled = true;
    ui.serviceStopButton.disabled = true;
    state.serviceSnapshot = null;
    ui.checkButton.disabled = true;
    ui.workspaceState.textContent = "尚未创建";
    updateDirtyState();
  }

  async function selectTask(taskId) {
    if (state.task && state.task.service_profile && state.serviceSnapshot &&
        ["STARTING", "RUNNING"].includes(state.serviceSnapshot.state)) {
      await stopNetworkService(true);
    }
    if (state.dirtyFiles.size && !await saveAllDirtyFiles({ silent: true })) {
      toast("当前代码未能保存，暂不切换课程。");
      return;
    }
    state.selectedTaskId = taskId;
    state.serviceClients = [];
    state.serviceMessages = [];
    state.task = null;
    clearTaskEditorState();
    ui.welcome.hidden = true;
    ui.workbench.hidden = false;
    ui.lesson.innerHTML = '<div class="loading">正在读取课程讲解…</div>';
    ui.hint.hidden = true;
    ui.console.textContent = "开始任务后，可以在这里查看构建、运行与验收输出。";
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "等待执行";
    ui.checkResult.className = "check-result";
    renderCourseList();
    try {
      const task = await api("/api/v1/tasks/" + encodeURIComponent(taskId));
      state.task = task;
      renderApplicationRunner();
      renderServiceRunner();
      await refreshServiceStatus(true);
      ui.taskBreadcrumb.textContent = task.id;
      ui.taskTitle.textContent = task.title;
      ui.taskSubtitle.textContent = "W" + task.week + " · " + task.source_files.length + " 个源文件 · " +
        (task.acceptance_kind === "exact_stdout" ? "单文件 C++ 程序 · 构建 / 运行 / 验收" : "CMake / GoogleTest 工程 · 课程测试 / 正式验收");
      ui.buildButton.textContent = task.acceptance_kind === "exact_stdout" ? "构建" : "构建测试工程";
      ui.runButton.textContent = task.acceptance_kind === "exact_stdout" ? "运行程序" : "运行课程测试";
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
      updateTaskState(state.task.id, state.task.state === "PASSED" ? "PASSED" : "IN PROGRESS");
      state.workspaceReady = true;
      ui.workspaceState.textContent = result.resumed ? "已恢复" : "已创建";
      ui.startButton.textContent = "工作区已打开";
      await loadWorkspaceFiles();
      toast(result.resumed ? "已恢复上次的学习工作区。" : "学习工作区已创建。");
    } catch (error) {
      toast(error.message);
      ui.console.textContent = error.message;
    } finally {
      setBusy(false);
    }
  }

  function declaredSourcePath(pathText) {
    const normalized = String(pathText).replace(/\\/g, "/");
    return state.files.map((file) => file.path).find((path) =>
      normalized === path || normalized.endsWith("/" + path)
    ) || null;
  }

  function renderDiagnostics(output) {
    ui.diagnostics.replaceChildren();
    const byPath = new Map();
    const diagnostics = [];
    const pattern = /^(.+?):(\d+):(\d+):\s*(fatal error|error|warning|note):\s*(.*)$/gm;
    let match;
    while ((match = pattern.exec(output)) !== null) {
      const path = declaredSourcePath(match[1]);
      const severity = match[4].includes("error") ? "error" :
        match[4] === "warning" ? "warning" : "note";
      const item = {
        path,
        line: Number(match[2]),
        column: Number(match[3]),
        severity,
        message: match[5],
        source: match[1]
      };
      diagnostics.push(item);
      if (path) {
        if (!byPath.has(path)) byPath.set(path, []);
        byPath.get(path).push({
          startLineNumber: item.line,
          startColumn: item.column,
          endLineNumber: item.line,
          endColumn: item.column + 1,
          message: item.message,
          severity: severity === "error" ? state.monaco.MarkerSeverity.Error :
            severity === "warning" ? state.monaco.MarkerSeverity.Warning :
            state.monaco.MarkerSeverity.Info
        });
      }
    }
    if (state.monaco) {
      for (const [path, model] of state.models) {
        state.monaco.editor.setModelMarkers(model, "cppapplab", byPath.get(path) || []);
      }
    }
    for (const item of diagnostics) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "diagnostic-button";
      const severity = document.createElement("span");
      severity.className = "diagnostic-severity " + item.severity;
      severity.textContent = item.severity === "error" ? "错误" : item.severity === "warning" ? "警告" : "提示";
      const location = document.createElement("span");
      location.className = "diagnostic-location";
      location.textContent = item.path ? item.path + ":" + item.line + ":" + item.column :
        item.source + ":" + item.line + ":" + item.column;
      const message = document.createElement("span");
      message.className = "diagnostic-message";
      message.textContent = item.message;
      button.append(severity, location, message);
      button.disabled = !item.path;
      if (item.path) {
        button.addEventListener("click", () => openDiagnostic(item.path, item.line, item.column));
      }
      ui.diagnostics.append(button);
    }
    ui.diagnostics.hidden = diagnostics.length === 0;
  }

  async function openDiagnostic(path, line, column) {
    if (path !== state.activeFile) await loadFile(path);
    renderDiagnostics(ui.console.textContent || "");
    if (state.editorInstance) {
      state.editorInstance.setPosition({ lineNumber: line, column });
      state.editorInstance.revealLineInCenter(line);
      state.editorInstance.focus();
    } else {
      ui.editor.focus();
    }
  }

  function setBusy(value, saveLabel) {
    state.busy = value;
    ui.startButton.disabled = value || !state.task || state.task.state === "LOCKED";
    ui.saveButton.disabled = value || !state.workspaceReady || state.dirtyFiles.size === 0;
    ui.buildButton.disabled = value || !state.workspaceReady;
    ui.runButton.disabled = value || !state.workspaceReady;
    const hasApplicationProfile = Boolean(state.task && state.task.runtime_profile);
    ui.appBuildButton.disabled = value || !state.workspaceReady || !hasApplicationProfile;
    ui.appExecuteButton.disabled = value || !state.workspaceReady || !hasApplicationProfile;
    ui.appAction.disabled = value || !hasApplicationProfile;
    ui.appFields.querySelectorAll("input, select, textarea").forEach((control) => {
      control.disabled = value;
    });
    ui.checkButton.disabled = value || !state.workspaceReady;
    if (state.task && state.task.service_profile) {
      updateServiceSnapshot(state.serviceSnapshot || { state: "STOPPED", port: null, logs: "" });
    }
    if (saveLabel) ui.saveState.textContent = saveLabel;
  }

  function updateApplicationGuidance() {
    if (!state.task || !state.task.runtime_profile) return;
    if (state.task.state === "LOCKED") {
      ui.appRunnerHelp.textContent = "当前课程尚未解锁。先完成前置课程的正式验收，才能构建和运行此程序。";
    } else if (!state.workspaceReady) {
      ui.appRunnerHelp.textContent = "先点击“开始学习”创建工作区，然后构建应用、选择操作并运行。";
    } else {
      ui.appRunnerHelp.textContent = "运行的是当前工作区里的学习者代码。输出和错误显示在下方，不会改变正式验收状态。";
    }
  }

  function renderApplicationRunner() {
    const profile = state.task && state.task.runtime_profile;
    if (!profile) {
      ui.appRunner.hidden = true;
      ui.appBuildButton.disabled = true;
      ui.appExecuteButton.disabled = true;
      return;
    }
    ui.appRunner.hidden = false;
    ui.appProfileLabel.textContent = profile.label || "固定应用配置";
    updateApplicationGuidance();
    ui.appAction.replaceChildren();
    for (const action of profile.actions || []) {
      const option = document.createElement("option");
      option.value = action.id;
      option.textContent = action.label || action.id;
      ui.appAction.append(option);
    }
    renderApplicationFields();
    ui.appBuildButton.disabled = !state.workspaceReady || state.busy;
    ui.appExecuteButton.disabled = !state.workspaceReady || state.busy;
  }

  function renderApplicationFields() {
    ui.appFields.replaceChildren();
    const profile = state.task && state.task.runtime_profile;
    if (!profile) return;
    const action = (profile.actions || []).find((item) => item.id === ui.appAction.value);
    if (!action) return;
    for (const field of action.fields || []) {
      const label = document.createElement("label");
      label.textContent = field.label || field.name;
      let control;
      if (field.type === "select") {
        control = document.createElement("select");
        for (const value of field.options || []) {
          const option = document.createElement("option");
          option.value = value;
          option.textContent = (field.option_labels && field.option_labels[value]) || value;
          control.append(option);
        }
      } else if (field.type === "multiline") {
        control = document.createElement("textarea");
        control.rows = 5;
        control.maxLength = 262144;
        control.value = field.default || "";
        control.placeholder = field.placeholder || "";
      } else {
        control = document.createElement("input");
        control.type = field.type === "number" ? "number" : field.type === "id" ? "number" : "text";
        if (field.type === "number") control.step = "any";
        if (field.type === "id") {
          control.min = "1";
          control.step = "1";
        }
        if (field.type === "text") control.maxLength = 200;
      }
      control.dataset.field = field.name;
      control.dataset.fieldType = field.type;
      control.autocomplete = "off";
      control.required = true;
      label.append(control);
      ui.appFields.append(label);
    }
  }

  function serviceStateLabel(snapshot) {
    const stateValue = snapshot && snapshot.state ? snapshot.state : "STOPPED";
    if (stateValue === "STOPPED" && snapshot && snapshot.exit_code === 0) return "已完成";
    return ({
      STOPPED: "未启动",
      STARTING: "启动中",
      RUNNING: "运行中",
      STOPPING: "停止中",
      FAILED: "启动失败",
      BUILD_FAILED: "构建失败",
      EXPIRED: "已过期"
    })[stateValue] || stateValue;
  }

  function updateServiceSnapshot(snapshot) {
    state.serviceSnapshot = snapshot || { state: "STOPPED", port: null, logs: "" };
    if (!state.task || !state.task.service_profile) return;
    const current = state.serviceSnapshot;
    ui.serviceState.textContent = serviceStateLabel(current);
    ui.serviceState.dataset.state = current.state || "STOPPED";
    ui.servicePort.textContent = current.port ? "127.0.0.1:" + current.port : "";
    ui.serviceLogs.textContent = current.logs || "暂无服务日志。";
    const locked = state.task.state === "LOCKED";
    const active = ["STARTING", "RUNNING", "STOPPING"].includes(current.state);
    ui.serviceStartButton.disabled = state.busy || !state.workspaceReady || locked || active;
    ui.serviceStopButton.disabled = state.busy || !active;
    ui.serviceRefreshButton.disabled = state.busy;
    if (!active && ["STOPPED", "FAILED", "BUILD_FAILED", "EXPIRED"].includes(current.state)) {
      state.serviceClients = [];
      renderChatClientOptions();
    }
    ui.serviceControls.querySelectorAll("input, select, textarea, button").forEach((control) => {
      const unavailable = state.busy || !state.workspaceReady || locked || !active;
      const noChatClient = ["service-chat-send", "service-chat-poll", "service-chat-disconnect"].includes(control.id) &&
        state.serviceClients.length === 0;
      control.disabled = unavailable || noChatClient ||
        (control.id === "service-http-body" &&
          ui.serviceControls.querySelector("#service-http-method")?.value !== "POST");
    });
    const chatOutput = ui.serviceControls.querySelector("#service-chat-output");
    if (chatOutput) chatOutput.textContent = state.serviceMessages.join("\n") || "连接客户端后，聊天消息会显示在这里。";
  }

  function renderServiceRunner() {
    const profile = state.task && state.task.service_profile;
    if (!profile) {
      ui.serviceRunner.hidden = true;
      ui.serviceControls.replaceChildren();
      state.serviceSnapshot = null;
      return;
    }
    ui.serviceRunner.hidden = false;
    ui.serviceProfileLabel.textContent = profile.label || "网络服务";
    if (state.task.state === "LOCKED") {
      ui.serviceRunnerHelp.textContent = "当前课程尚未解锁。完成前置课程的正式验收后，才能启动网络服务。平台连接桥只访问 127.0.0.1；学习者服务应保持回环绑定。";
    } else if (!state.workspaceReady) {
      ui.serviceRunnerHelp.textContent = "先打开学习工作区，再启动当前任务的真实服务进程。平台不会接受浏览器指定的主机、端口或可执行文件。";
    } else if (profile.kind === "tcp_echo") {
      ui.serviceRunnerHelp.textContent = "启动当前工作区中的 TCP Echo 服务，发送一条消息后由学习者客户端连接并验证真实回显。";
    } else if (profile.kind === "tcp_chat") {
      ui.serviceRunnerHelp.textContent = "启动当前工作区中的多客户端 TCP Chat 服务。所有客户端连接均由本机工作台管理。";
    } else {
      ui.serviceRunnerHelp.textContent = "启动当前工作区中的 HTTP 服务。浏览器请求将通过本机工作台代理到该任务的回环监听端口。";
    }
    ui.serviceControls.replaceChildren();
    if (profile.kind === "tcp_echo") {
      const label = document.createElement("label");
      label.textContent = "发送消息";
      const input = document.createElement("input");
      input.id = "service-echo-message";
      input.maxLength = 1024;
      input.autocomplete = "off";
      input.placeholder = "例如：Hello, TCP Echo!";
      label.append(input);
      const send = document.createElement("button");
      send.id = "service-echo-send";
      send.type = "button";
      send.className = "primary-button compact";
      send.textContent = "发送并验证回显";
      send.addEventListener("click", sendTcpEchoMessage);
      ui.serviceControls.append(label, send);
    } else if (profile.kind === "tcp_chat") {
      const nicknameLabel = document.createElement("label");
      nicknameLabel.textContent = "客户端昵称";
      const nickname = document.createElement("input");
      nickname.id = "service-chat-nickname";
      nickname.maxLength = 24;
      nickname.autocomplete = "off";
      nickname.placeholder = "例如：Alice";
      nicknameLabel.append(nickname);

      const connect = document.createElement("button");
      connect.id = "service-chat-connect";
      connect.type = "button";
      connect.className = "secondary-button";
      connect.textContent = "连接新客户端";
      connect.addEventListener("click", connectChatClient);

      const clientLabel = document.createElement("label");
      clientLabel.textContent = "当前客户端";
      const clientSelect = document.createElement("select");
      clientSelect.id = "service-chat-client";
      clientLabel.append(clientSelect);

      const messageLabel = document.createElement("label");
      messageLabel.textContent = "聊天消息";
      const message = document.createElement("input");
      message.id = "service-chat-message";
      message.maxLength = 1024;
      message.autocomplete = "off";
      message.placeholder = "输入一条消息";
      messageLabel.append(message);

      const send = document.createElement("button");
      send.id = "service-chat-send";
      send.type = "button";
      send.className = "primary-button compact";
      send.textContent = "发送消息";
      send.addEventListener("click", sendChatMessage);

      const poll = document.createElement("button");
      poll.id = "service-chat-poll";
      poll.type = "button";
      poll.className = "secondary-button";
      poll.textContent = "刷新消息";
      poll.addEventListener("click", pollChatMessages);

      const disconnect = document.createElement("button");
      disconnect.id = "service-chat-disconnect";
      disconnect.type = "button";
      disconnect.className = "secondary-button";
      disconnect.textContent = "断开客户端";
      disconnect.addEventListener("click", disconnectChatClient);

      const output = document.createElement("pre");
      output.id = "service-chat-output";
      output.className = "service-chat-output";
      output.textContent = state.serviceMessages.join("\n") || "连接客户端后，聊天消息会显示在这里。";
      ui.serviceControls.append(
        nicknameLabel, connect, clientLabel, messageLabel, send, poll, disconnect, output
      );
      renderChatClientOptions();
    } else {
      const methodLabel = document.createElement("label");
      methodLabel.textContent = "HTTP 方法";
      const method = document.createElement("select");
      method.id = "service-http-method";
      for (const value of ["GET", "HEAD", "POST"]) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = value;
        method.append(option);
      }
      method.addEventListener("change", () => {
        const body = ui.serviceControls.querySelector("#service-http-body");
        const active = state.serviceSnapshot && state.serviceSnapshot.state === "RUNNING";
        if (body) body.disabled =
          method.value !== "POST" || state.busy || !state.workspaceReady || !active ||
          state.task.state === "LOCKED";
      });
      methodLabel.append(method);

      const targetLabel = document.createElement("label");
      targetLabel.textContent = "请求路径";
      const target = document.createElement("input");
      target.id = "service-http-target";
      target.value = "/";
      target.placeholder = "/index.html";
      target.maxLength = 2048;
      targetLabel.append(target);

      const bodyLabel = document.createElement("label");
      bodyLabel.textContent = "请求正文（POST）";
      const body = document.createElement("textarea");
      body.id = "service-http-body";
      body.placeholder = "可选的 UTF-8 请求正文";
      body.maxLength = 65536;
      body.disabled = true;
      bodyLabel.append(body);

      const send = document.createElement("button");
      send.id = "service-http-send";
      send.type = "button";
      send.className = "primary-button compact";
      send.textContent = "发送 HTTP 请求";
      send.addEventListener("click", sendHttpServiceRequest);
      ui.serviceControls.append(methodLabel, targetLabel, bodyLabel, send);
    }
    updateServiceSnapshot(state.serviceSnapshot || { state: "STOPPED", port: null, logs: "" });
  }

  async function refreshServiceStatus(silent = false) {
    if (!state.task || !state.task.service_profile) return;
    try {
      const snapshot = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/status"
      );
      updateServiceSnapshot(snapshot);
      return snapshot;
    } catch (error) {
      if (!silent) toast(error.message);
      return null;
    }
  }

  async function startNetworkService() {
    if (!state.task || !state.task.service_profile || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存代码…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消服务启动。");
      return;
    }
    try {
      const snapshot = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/start",
        { method: "POST", body: {} }
      );
      updateServiceSnapshot(snapshot);
      if (snapshot.state === "RUNNING") {
        toast("网络服务已启动；进程由本机工作台管理。");
      } else {
        toast("网络服务未能启动，请查看服务日志中的真实构建或运行诊断。");
      }
    } catch (error) {
      toast(error.message);
      ui.serviceLogs.textContent = error.message;
    } finally {
      setBusy(false);
    }
  }

  async function stopNetworkService(silent = false) {
    if (!state.task || !state.task.service_profile || state.busy) return;
    try {
      setBusy(true);
      const snapshot = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/stop",
        { method: "POST", body: {} }
      );
      updateServiceSnapshot(snapshot);
      if (!silent) toast("网络服务已停止，子进程组已清理。");
    } catch (error) {
      if (!silent) toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function sendTcpEchoMessage() {
    if (!state.task || !state.task.service_profile || state.task.service_profile.kind !== "tcp_echo" || state.busy) return;
    const input = ui.serviceControls.querySelector("#service-echo-message");
    const message = input ? input.value : "";
    if (!message.trim()) {
      toast("请输入要发送的消息。");
      return;
    }
    setBusy(true, "正在连接 TCP Echo 服务…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/echo",
        { method: "POST", body: { message } }
      );
      updateServiceSnapshot(result.service);
      ui.consoleTitle.textContent = "TCP Echo 客户端输出";
      ui.console.textContent = result.output || "(没有输出)";
      ui.checkResult.textContent = result.success ? "回显成功" : "回显失败";
      ui.checkResult.className = "check-result " + (result.success ? "pass" : "fail");
      toast(result.success ? "已收到学习者 TCP Echo 服务的真实回显。" : "TCP Echo 客户端执行失败，请查看输出和服务日志。");
    } catch (error) {
      toast(error.message);
      ui.consoleTitle.textContent = "TCP Echo 客户端输出";
      ui.console.textContent = error.message;
    } finally {
      setBusy(false);
    }
  }

  function renderChatClientOptions() {
    const select = ui.serviceControls.querySelector("#service-chat-client");
    if (!select) return;
    const previous = select.value;
    select.replaceChildren();
    for (const client of state.serviceClients) {
      const option = document.createElement("option");
      option.value = client.id;
      option.textContent = client.nickname;
      select.append(option);
    }
    if (state.serviceClients.some((client) => client.id === previous)) {
      select.value = previous;
    }
  }

  function appendChatMessages(messages) {
    if (!messages || typeof messages !== "object") return;
    for (const [clientId, values] of Object.entries(messages)) {
      const client = state.serviceClients.find((item) => item.id === clientId);
      const nickname = client ? client.nickname : "服务端";
      for (const value of values || []) {
        state.serviceMessages.push(nickname + "  " + value);
      }
    }
    if (state.serviceMessages.length > 100) {
      state.serviceMessages = state.serviceMessages.slice(-100);
    }
    const output = ui.serviceControls.querySelector("#service-chat-output");
    if (output) output.textContent = state.serviceMessages.join("\n") || "暂无聊天消息。";
  }

  async function connectChatClient() {
    if (!state.task || !state.serviceSnapshot || state.serviceSnapshot.state !== "RUNNING" || state.busy) return;
    const input = ui.serviceControls.querySelector("#service-chat-nickname");
    const nickname = input ? input.value.trim() : "";
    if (!nickname) {
      toast("请先填写客户端昵称。");
      return;
    }
    setBusy(true, "正在连接 TCP Chat 客户端…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/chat/connect",
        { method: "POST", body: { nickname } }
      );
      state.serviceClients.push({ id: result.client_id, nickname: result.nickname });
      appendChatMessages(result.messages);
      renderChatClientOptions();
      updateServiceSnapshot(result.service);
      if (input) input.value = "";
      toast("客户端 " + nickname + " 已连接。可以再连接另一个昵称进行多客户端测试。");
    } catch (error) {
      toast(error.message);
      ui.consoleTitle.textContent = "TCP Chat 连接结果";
      ui.console.textContent = error.message;
    } finally {
      setBusy(false);
    }
  }

  async function sendChatMessage() {
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const input = ui.serviceControls.querySelector("#service-chat-message");
    const clientId = select ? select.value : "";
    const message = input ? input.value : "";
    if (!clientId || !message.trim()) {
      toast("请选择客户端并填写消息。");
      return;
    }
    setBusy(true, "正在发送 TCP Chat 消息…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/chat/send",
        { method: "POST", body: { client_id: clientId, message } }
      );
      appendChatMessages(result.messages);
      updateServiceSnapshot(result.service);
      if (input) input.value = "";
    } catch (error) {
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function pollChatMessages() {
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const clientId = select ? select.value : "";
    if (!clientId) {
      toast("请先连接一个 Chat 客户端。");
      return;
    }
    setBusy(true, "正在刷新 TCP Chat 消息…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/chat/poll",
        { method: "POST", body: { client_id: clientId } }
      );
      appendChatMessages(result.messages);
      updateServiceSnapshot(result.service);
    } catch (error) {
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function disconnectChatClient() {
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const clientId = select ? select.value : "";
    const client = state.serviceClients.find((item) => item.id === clientId);
    if (!clientId || !client) {
      toast("请选择需要断开的客户端。");
      return;
    }
    setBusy(true, "正在断开 TCP Chat 客户端…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/chat/disconnect",
        { method: "POST", body: { client_id: clientId } }
      );
      appendChatMessages(result.messages);
      state.serviceClients = state.serviceClients.filter((item) => item.id !== clientId);
      renderChatClientOptions();
      updateServiceSnapshot(result.service);
    } catch (error) {
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function sendHttpServiceRequest() {
    if (!state.task || state.busy) return;
    const method = ui.serviceControls.querySelector("#service-http-method")?.value || "GET";
    const target = ui.serviceControls.querySelector("#service-http-target")?.value || "/";
    const body = ui.serviceControls.querySelector("#service-http-body")?.value || "";
    setBusy(true, "正在向学习者 HTTP Server 发送请求…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/http",
        { method: "POST", body: { method, target, body } }
      );
      updateServiceSnapshot(result.service);
      ui.consoleTitle.textContent = "HTTP 请求 / 响应";
      ui.console.textContent = [
        method + " " + target,
        "HTTP " + result.status_code + " " + result.reason,
        "Response headers:",
        JSON.stringify(result.headers, null, 2),
        "",
        "Response body (" + result.body_bytes + " bytes):",
        result.body
      ].join("\n");
      ui.checkResult.textContent = "HTTP " + result.status_code;
      ui.checkResult.className = "check-result " + (result.status_code >= 200 && result.status_code < 400 ? "pass" : "fail");
    } catch (error) {
      ui.consoleTitle.textContent = "HTTP 请求 / 响应";
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "请求失败";
      ui.checkResult.className = "check-result fail";
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function runApplicationBuild() {
    if (!state.task || !state.task.runtime_profile || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存代码…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消应用构建。");
      return;
    }
    ui.consoleTitle.textContent = "应用程序构建输出";
    ui.console.textContent = "正在构建当前任务的固定应用配置…";
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "执行中";
    ui.checkResult.className = "check-result";
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/app-build",
        { method: "POST", body: {} }
      );
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      ui.checkResult.textContent = result.success ? "应用构建成功" : "应用构建失败";
      ui.checkResult.className = "check-result " + (result.success ? "pass" : "fail");
      toast(result.success ? "应用程序构建成功；正式验收状态未改变。" : "应用构建失败，请查看编译诊断。");
    } catch (error) {
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "应用构建失败";
      ui.checkResult.className = "check-result fail";
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function runApplication() {
    if (!state.task || !state.task.runtime_profile || !state.workspaceReady || state.busy) return;
    const profile = state.task.runtime_profile;
    const action = (profile.actions || []).find((item) => item.id === ui.appAction.value);
    if (!action) {
      toast("当前任务没有可运行的应用操作。");
      return;
    }
    const values = Array.from(ui.appFields.querySelectorAll("[data-field]"))
      .map((control) => control.dataset.fieldType === "multiline"
        ? control.value.replace(/\r?\n/g, "\r\n")
        : control.value);
    setBusy(true, "正在保存代码…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消程序运行。");
      return;
    }
    ui.consoleTitle.textContent = "程序交互输出";
    ui.console.textContent = "正在执行：" + (action.label || action.id);
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "执行中";
    ui.checkResult.className = "check-result";
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/app-run",
        { method: "POST", body: { action: action.id, values } }
      );
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      ui.checkResult.textContent = result.success ? "程序运行成功" : "程序运行失败";
      ui.checkResult.className = "check-result " + (result.success ? "pass" : "fail");
      toast(result.success
        ? "程序运行完成；没有写入正式验收记录。"
        : "程序运行失败；请根据输出修改代码后重试。");
    } catch (error) {
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "程序运行失败";
      ui.checkResult.className = "check-result fail";
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function runWorkspaceAction(action) {
    if (!state.task || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存代码…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消本次操作。");
      return;
    }
    const labels = {
      build: "正在构建项目…",
      run: "正在运行程序 / 测试…",
      check: "正在执行平台固定验收…"
    };
    ui.consoleTitle.textContent = action === "build" ? "构建输出" :
      action === "run" ? (state.task.acceptance_kind === "exact_stdout" ? "程序运行输出" : "测试运行输出") :
      "平台固定验收输出";
    ui.console.textContent = labels[action];
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "执行中";
    ui.checkResult.className = "check-result";
    setBusy(true, labels[action]);
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/" + action,
        { method: "POST", body: {} }
      );
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      const passed = result.passed;
      ui.checkResult.textContent = action === "check" ? (passed ? "PASS" : "未通过") :
        action === "build" ? (passed ? "构建成功" : "构建失败") :
        (passed ? "执行成功" : "执行失败");
      ui.checkResult.className = "check-result " + (passed ? "pass" : "fail");
      if (action === "check") {
        updateTaskState(state.task.id, passed ? "PASSED" : "NEEDS WORK");
        if (passed) {
          try {
            // Refresh prerequisite-derived states so the next task unlocks immediately.
            await loadTasks();
          } catch (refreshError) {
            toast("验收已记录，但课程状态刷新失败：" + refreshError.message);
          }
        }
        toast(passed ? "平台固定验收已通过，下一任务已更新。" : "验收未通过，请根据诊断继续修改。");
      } else if (action === "build") {
        toast(passed ? "构建成功。尚未记录正式验收结果。" : "构建失败，请查看编译诊断。");
      } else {
        toast(passed ? "运行 / 测试完成。正式验收需要单独执行。" : "运行 / 测试未通过，请查看输出。");
      }
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

  function bindEvents() {
    ui.search.addEventListener("input", renderCourseList);
    ui.startButton.addEventListener("click", startTask);
    ui.saveButton.addEventListener("click", () => saveAllDirtyFiles());
    ui.buildButton.addEventListener("click", () => runWorkspaceAction("build"));
    ui.runButton.addEventListener("click", () => runWorkspaceAction("run"));
    ui.appBuildButton.addEventListener("click", runApplicationBuild);
    ui.appExecuteButton.addEventListener("click", runApplication);
    ui.appAction.addEventListener("change", renderApplicationFields);
    ui.serviceStartButton.addEventListener("click", startNetworkService);
    ui.serviceRefreshButton.addEventListener("click", () => refreshServiceStatus());
    ui.serviceStopButton.addEventListener("click", () => stopNetworkService());
    ui.checkButton.addEventListener("click", () => runWorkspaceAction("check"));
    ui.historyButton.addEventListener("click", () => {
      if (state.task) loadTaskHistory(state.task.id);
    });
    ui.historyClose.addEventListener("click", () => { ui.historyPanel.hidden = true; });
    ui.continueButton.addEventListener("click", () => {
      const next = state.tasks.find((task) => ["IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state))
        || state.tasks.find((task) => task.state === "AVAILABLE");
      if (next) selectTask(next.id);
    });
    ui.editor.addEventListener("input", handleEditorInput);
    ui.editor.addEventListener("keydown", (event) => {
      if (event.key === "Tab") {
        event.preventDefault();
        const start = ui.editor.selectionStart;
        const end = ui.editor.selectionEnd;
        ui.editor.setRangeText("    ", start, end, "end");
        handleEditorInput();
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        saveAllDirtyFiles();
      }
    });
    document.querySelectorAll("[data-hint]").forEach((button) => {
      button.addEventListener("click", () => showHint(Number(button.dataset.hint)));
    });
    window.addEventListener("beforeunload", (event) => {
      if (state.dirtyFiles.size) {
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
