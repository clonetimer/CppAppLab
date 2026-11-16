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
    autosaveTimer: null
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
    monacoContainer: $("monaco-editor"),
    editorEngineStatus: $("editor-engine-status"),
    buildButton: $("build-button"),
    runButton: $("run-button"),
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
                fontSize: 13,
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
    ui.checkButton.disabled = true;
    ui.workspaceState.textContent = "尚未创建";
    updateDirtyState();
  }

  async function selectTask(taskId) {
    if (state.dirtyFiles.size && !await saveAllDirtyFiles({ silent: true })) {
      toast("当前代码未能保存，暂不切换课程。");
      return;
    }
    state.selectedTaskId = taskId;
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
      ui.taskBreadcrumb.textContent = task.id;
      ui.taskTitle.textContent = task.title;
      ui.taskSubtitle.textContent = "W" + task.week + " · " + task.source_files.length + " 个源文件 · " +
        (task.acceptance_kind === "exact_stdout" ? "单文件 C++ 程序 · 构建 / 运行 / 验收" : "CMake / GoogleTest 工程 · 构建 / 测试 / 验收");
      ui.runButton.textContent = task.acceptance_kind === "exact_stdout" ? "运行程序" : "运行测试";
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
    ui.checkButton.disabled = value || !state.workspaceReady;
    if (saveLabel) ui.saveState.textContent = saveLabel;
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
        toast(passed ? "平台固定验收已通过，学习记录已更新。" : "验收未通过，请根据诊断继续修改。");
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
    ui.checkButton.addEventListener("click", () => runWorkspaceAction("check"));
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
