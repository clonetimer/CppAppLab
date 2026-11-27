(() => {
  "use strict";

  const MONACO_VERSION = "0.52.2";
  const LEARNING_POSITION_KEY = "cppapplab.learning-position.v1";
  const LESSON_LANGUAGE_KEY = "cppapplab.lesson-language.v1";
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
    lessonLanguage: readLessonLanguage(),
    hintDocument: null,
    files: [],
    activeFile: "",
    workspaceReady: false,
    workspaceError: "",
    preferredFile: "",
    learningPosition: null,
    learningHistory: null,
    restoringHistory: false,
    pendingLocationMode: "",
    navigatingTask: false,
    dirtyFiles: new Set(),
    savingFiles: new Map(),
    fileContents: new Map(),
    fileVersions: new Map(),
    models: new Map(),
    monaco: null,
    editorInstance: null,
    editorReady: null,
    editorFallback: null,
    busy: false,
    busyLabel: "",
    progressFresh: false,
    lastAction: null,
    catalogRequest: 0,
    hintRequest: 0,
    historyRequest: 0,
    fileRequest: 0,
    serviceRequest: 0,
    settingEditorValue: false,
    autosaveTimer: null,
    serviceSnapshot: null,
    serviceClients: [],
    serviceMessages: [],
    guideMarkdown: null,
    guideRequest: null,
    guideOpener: null,
    guideSession: 0
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
    resumeButton: $("resume-button"),
    guideButton: $("guide-button"),
    welcomeGuide: $("welcome-guide"),
    guideDialog: $("learning-guide"),
    guideClose: $("guide-close"),
    guideSections: $("guide-sections"),
    guideStatus: $("guide-status"),
    guideRetry: $("guide-retry"),
    guideContent: $("guide-content"),
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
    lessonLanguage: $("lesson-language"),
    lessonLanguageStatus: $("lesson-language-status"),
    hint: $("hint-content"),
    hintPanel: $("hint-panel"),
    hintTitle: $("hint-title"),
    hintClose: $("hint-close"),
    workflowHelp: $("workflow-help"),
    workflowAction: $("workflow-action"),
    fileList: $("file-list"),
    activeFile: $("active-file"),
    editor: $("source-editor"),
    monacoContainer: $("monaco-editor"),
    editorEngineStatus: $("editor-engine-status"),
    editorLoading: $("editor-loading"),
    editorFallbackButton: $("editor-fallback-button"),
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
    consoleToggle: $("console-toggle"),
    consoleSize: $("console-size"),
    consoleSummary: $("console-summary"),
    consoleDock: $("console-dock"),
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

    // Only recognize a table after a matching delimiter row. Pipes in code or
    // escaped as \| are cell content, and malformed rows remain ordinary text.
    const tableCells = (line) => {
      const row = line.trim();
      const cells = [];
      let cell = "";
      let codeEnd = -1;
      for (let index = 0; index < row.length; index += 1) {
        const character = row[index];
        if (character === "\\" && ["\\", "|"].includes(row[index + 1])) {
          cell += row[index + 1] === "|" ? "|" : "\\\\";
          index += 1;
        } else if (character === "`" && index >= codeEnd) {
          const delimiter = row.slice(index).match(/^`+/)[0];
          let end = row.indexOf(delimiter, index + delimiter.length);
          while (end !== -1 && (row[end - 1] === "`" || row[end + delimiter.length] === "`")) {
            end = row.indexOf(delimiter, end + delimiter.length);
          }
          codeEnd = end === -1 ? -1 : end + delimiter.length;
          cell += delimiter;
          index += delimiter.length - 1;
        } else if (character === "|" && index >= codeEnd) {
          cells.push(cell);
          cell = "";
        } else {
          cell += character;
        }
      }
      if (!cells.length) return null;
      cells.push(cell);
      if (cells[0] === "") cells.shift();
      if (cells[cells.length - 1] === "") cells.pop();
      return cells.map((value) => value.trim());
    };

    for (let index = 0; index < lines.length; index += 1) {
      const line = lines[index];
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
      const headers = tableCells(line);
      const delimiters = headers && index + 1 < lines.length ? tableCells(lines[index + 1]) : null;
      if (headers && headers.length && delimiters && headers.length === delimiters.length &&
          delimiters.every((cell) => /^:?-{3,}:?$/.test(cell))) {
        flushText();
        const rows = [];
        index += 1;
        while (index + 1 < lines.length) {
          const cells = tableCells(lines[index + 1]);
          if (!cells || cells.length !== headers.length) break;
          rows.push("<tr>" + cells.map((cell) => "<td>" + renderInline(cell) + "</td>").join("") + "</tr>");
          index += 1;
        }
        output.push('<div class="markdown-table-scroll" role="region" aria-label="课程表格（可横向滚动）" tabindex="0"><table><thead><tr>' +
          headers.map((cell) => '<th scope="col">' + renderInline(cell) + "</th>").join("") +
          "</tr></thead><tbody>" + rows.join("") + "</tbody></table></div>");
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
      "LINUX/WSL2 REQUIRED": "需要 Linux / WSL2",
      "UNKNOWN": "状态未知"
    })[value] || value || "未知";
  }

  function environmentBlocked(task) {
    return task && (task.execution_available === false || task.state === "LINUX/WSL2 REQUIRED");
  }

  function readLessonLanguage() {
    try { return window.localStorage.getItem(LESSON_LANGUAGE_KEY) === "en" ? "en" : "zh-CN"; }
    catch { return "zh-CN"; }
  }

  function courseTitle(task) {
    return task.title_translations?.[state.lessonLanguage] || task.title;
  }

  function renderCourseDocument(element, original, translations) {
    const translated = translations?.[state.lessonLanguage];
    const available = typeof translated === "string" && translated.trim();
    const language = available ? state.lessonLanguage : "en";
    const oldScroll = element.scrollTop;
    element.innerHTML = renderMarkdown(available ? translated : original);
    element.lang = language;
    enhanceLessonFiles(element);
    element.scrollTop = oldScroll;
    return language;
  }

  function updateLessonLanguageStatus() {
    const content = ui.hintPanel.hidden ? ui.lesson : ui.hint;
    const loaded = ui.hintPanel.hidden || Boolean(state.hintDocument);
    const fallback = loaded && state.lessonLanguage === "zh-CN" && content.lang === "en";
    ui.lessonLanguageStatus.hidden = !fallback;
    ui.lessonLanguageStatus.textContent = fallback
      ? (ui.hintPanel.hidden ? "本课" : "此提示") + "暂无中文译文，当前显示英文原文。" : "";
  }

  function renderLesson() {
    if (!state.task) return;
    renderCourseDocument(ui.lesson, state.task.lesson_markdown, state.task.lesson_translations);
    updateLessonLanguageStatus();
  }

  function renderHint() {
    if (!state.hintDocument) return;
    renderCourseDocument(ui.hint, state.hintDocument.markdown, state.hintDocument.translations);
    updateLessonLanguageStatus();
  }

  function changeLessonLanguage(language) {
    if (!["zh-CN", "en"].includes(language)) {
      ui.lessonLanguage.value = state.lessonLanguage;
      return;
    }
    state.lessonLanguage = language;
    ui.lessonLanguage.value = language;
    try { window.localStorage.setItem(LESSON_LANGUAGE_KEY, language); }
    catch { /* Language switching still works when browser storage is unavailable. */ }
    if (state.task) ui.taskTitle.textContent = courseTitle(state.task);
    renderLesson();
    if (!ui.hintPanel.hidden) renderHint();
    renderCourseList();
    updateResumeAction();
    updateWorkflowGuidance();
  }

  function learningPosition() {
    if (state.learningPosition) return state.learningPosition;
    const position = { version: 1, lastTaskId: "", files: Object.create(null) };
    try {
      const raw = window.localStorage.getItem(LEARNING_POSITION_KEY);
      const saved = raw && raw.length <= 32768 ? JSON.parse(raw) : null;
      if (saved && saved.version === 1 && saved.files && typeof saved.files === "object" && !Array.isArray(saved.files)) {
        if (state.tasks.some((task) => task.id === saved.lastTaskId)) position.lastTaskId = saved.lastTaskId;
        for (const task of state.tasks) {
          const file = saved.files[task.id];
          if (typeof file === "string" && file.length <= 512) position.files[task.id] = file;
        }
      }
    } catch { /* Remembering presentation context is optional. */ }
    state.learningPosition = position;
    return position;
  }

  function updateResumeAction() {
    const position = learningPosition();
    const recent = state.tasks.find((task) => task.id === position.lastTaskId);
    ui.resumeButton.hidden = !recent;
    ui.resumeButton.disabled = state.busy || !state.progressFresh;
    ui.resumeButton.textContent = recent ? "返回上次：" + courseTitle(recent).replace(/^W\d+\s*[·:]\s*/, "") : "返回上次课程";
  }

  function rememberLearningPosition() {
    if (!state.task) return;
    const position = learningPosition();
    position.lastTaskId = state.task.id;
    if (state.files.some((file) => file.path === state.activeFile)) position.files[state.task.id] = state.activeFile;
    try { window.localStorage.setItem(LEARNING_POSITION_KEY, JSON.stringify(position)); }
    catch { /* The current URL still supports refresh and bookmarks. */ }
    updateResumeAction();
  }

  function learningHistoryEntry() {
    const entry = window.history && window.history.state && window.history.state.cppapplabLearning;
    return entry && typeof entry.scope === "string" && Number.isSafeInteger(entry.index) && entry.index >= 0
      ? entry : null;
  }

  function stampLearningHistory(entry, url, mode = "replace") {
    window.history[mode === "push" ? "pushState" : "replaceState"](
      { ...window.history.state, cppapplabLearning: entry }, "", url
    );
  }

  function ensureLearningHistory() {
    if (state.learningHistory || !window.history) return;
    state.learningHistory = learningHistoryEntry() || {
      scope: Date.now().toString(36) + Math.random().toString(36).slice(2), index: 0
    };
    try { stampLearningHistory(state.learningHistory, window.location.href); }
    catch { /* URL navigation remains optional when history is unavailable. */ }
  }

  function writeLearningLocation(mode = "replace") {
    if (!window.location || !window.history) return;
    ensureLearningHistory();
    if (state.restoringHistory) {
      if (!state.pendingLocationMode || mode === "push") state.pendingLocationMode = mode;
      return;
    }
    const parameters = new URLSearchParams();
    if (state.task) {
      parameters.set("task", state.task.id);
      const file = state.activeFile || state.preferredFile;
      if (file) parameters.set("file", file);
    }
    const hash = parameters.size ? "#" + parameters.toString() : "";
    try {
      const entry = learningHistoryEntry() || state.learningHistory;
      const push = mode === "push" && window.location.hash !== hash;
      const next = push ? { scope: entry.scope, index: entry.index + 1 } : entry;
      stampLearningHistory(next, window.location.pathname + window.location.search + hash, push ? "push" : "replace");
      state.learningHistory = next;
    } catch { /* Navigation within the workbench remains usable without history. */ }
  }

  function restoreLearningLocation() {
    const target = learningHistoryEntry();
    const current = state.learningHistory;
    if (target && current && target.scope === current.scope && target.index !== current.index) {
      try {
        state.restoringHistory = true;
        window.history.go(current.index - target.index);
        return;
      } catch { state.restoringHistory = false; }
    }
    writeLearningLocation();
  }

  async function showWelcome() {
    if (state.busy) return false;
    setBusy(true);
    try {
      if (state.dirtyFiles.size && !await saveAllDirtyFiles({ silent: true })) {
        toast("当前代码未能保存，暂不离开课程。");
        return false;
      }
      if (state.task && state.task.service_profile && state.serviceSnapshot &&
          ["STARTING", "RUNNING"].includes(state.serviceSnapshot.state)) await stopNetworkService(true, true);
      state.task = null;
      state.selectedTaskId = "";
      clearTaskEditorState();
      ui.welcome.hidden = false;
      ui.workbench.hidden = true;
      renderCourseList();
      writeLearningLocation();
      return true;
    } finally { setBusy(false); }
  }

  async function followLearningLocation() {
    ensureLearningHistory();
    const entry = learningHistoryEntry();
    if (state.restoringHistory) {
      if (entry && entry.scope === state.learningHistory.scope && entry.index === state.learningHistory.index) {
        state.restoringHistory = false;
        const mode = state.pendingLocationMode;
        state.pendingLocationMode = "";
        // The restored entry already describes the last committed position.
        // A destination may still be loading; only flush an actual completed write.
        if (mode) writeLearningLocation(mode);
      } else restoreLearningLocation();
      return false;
    }
    if (!entry || entry.scope !== state.learningHistory.scope) {
      // A newly entered fragment creates an untagged same-document entry.
      // Tag it before attempting navigation so rejection can restore the cursor.
      try { stampLearningHistory({ scope: state.learningHistory.scope, index: state.learningHistory.index + 1 }, window.location.href); }
      catch { /* The guarded file/course navigation still works without history. */ }
    }
    const parameters = new URLSearchParams(window.location.hash.slice(1));
    const taskId = parameters.get("task") || "";
    const filePath = parameters.get("file") || "";
    if (state.busy) {
      restoreLearningLocation();
      toast("当前课程正在处理，请等待完成后再切换。");
      return false;
    }
    if (!taskId && !window.location.hash) {
      const shown = await showWelcome();
      if (!shown) restoreLearningLocation();
      return shown;
    }
    if (!state.tasks.some((task) => task.id === taskId)) {
      toast("链接中的课程不可用，请从课程目录选择。");
      restoreLearningLocation();
      return false;
    }
    if (state.task && state.task.id === taskId) {
      if (!filePath || state.activeFile === filePath) {
        writeLearningLocation();
        return true;
      }
      if (!state.workspaceReady) {
        if (!state.activeFile) {
          state.preferredFile = filePath.slice(0, 512);
          writeLearningLocation();
          return true;
        }
        restoreLearningLocation();
        return false;
      }
      const requested = state.files.find((file) => file.path === filePath);
      const file = requested || state.files.find((item) => item.path === state.activeFile) || state.files[0];
      setBusy(true);
      try {
        const opened = Boolean(file) && await loadFile(file.path);
        if (!opened) restoreLearningLocation();
        else if (!requested) toast("链接中的文件不可用，已保留本课可用文件。");
        return opened;
      } finally { setBusy(false); }
    }
    const selected = await selectTask(taskId, {
      filePath: parameters.has("file") ? filePath.slice(0, 512) : undefined,
      historyMode: "replace"
    });
    if (!selected) restoreLearningLocation();
    return selected;
  }

  function updateProgressSummary() {
    const passed = state.tasks.filter((task) => task.state === "PASSED").length;
    const executable = state.tasks.filter(task => !environmentBlocked(task)).length;
    const nativeLimited = executable < state.tasks.length;
    ui.progress.textContent = passed + " / " + executable + " 个" + (nativeLimited ? "本机" : "") + "任务已通过" +
      (nativeLimited ? " · " + (state.tasks.length - executable) + " 项需 WSL2" : "");
    ui.taskCount.textContent = String(state.tasks.length);
  }

  function renderCourseList() {
    const query = ui.search.value.trim().toLocaleLowerCase();
    ui.courseList.replaceChildren();
    const weeks = new Map();
    for (const task of state.tasks) {
      const searchable = (task.id + " " + task.title + " " +
        Object.values(task.title_translations || {}).join(" ") + " W" + task.week).toLocaleLowerCase();
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
        button.disabled = state.busy;
        button.title = courseTitle(task);
        const marker = document.createElement("span");
        marker.className = "task-marker";
        marker.textContent = task.state === "PASSED" ? "✓" : (task.state === "LOCKED" || environmentBlocked(task)) ? "·" : task.id.split(".").slice(-1)[0].slice(0, 2).toUpperCase();
        const copy = document.createElement("span");
        copy.className = "task-link-copy";
        const title = document.createElement("strong");
        title.textContent = courseTitle(task).replace(/^W\d+\s*[·:]\s*/, "");
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
    updateWorkflowGuidance();
  }

  async function loadTaskHistory(taskId) {
    const request = ++state.historyRequest;
    ui.historyPanel.hidden = false;
    ui.historySummary.textContent = "正在读取验收记录…";
    ui.historyList.replaceChildren();
    try {
      const data = await api("/api/v1/tasks/" + encodeURIComponent(taskId) + "/history");
      if (!state.task || state.task.id !== taskId || request !== state.historyRequest) return;
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
      if (!state.task || state.task.id !== taskId || request !== state.historyRequest) return;
      ui.historySummary.textContent = "无法读取验收历史：" + error.message;
    }
  }

  async function loadTasks() {
    const request = ++state.catalogRequest;
    state.progressFresh = false;
    ui.continueButton.disabled = true;
    ui.continueButton.textContent = "正在刷新课程状态…";
    ui.startButton.disabled = true;
    updateWorkflowGuidance();
    let data;
    try {
      data = await api("/api/v1/tasks");
    } catch (error) {
      if (request !== state.catalogRequest) return false;
      ui.continueButton.textContent = "课程状态暂不可用";
      throw error;
    }
    if (request !== state.catalogRequest) return false;
    state.tasks = Array.isArray(data.tasks) ? data.tasks : [];
    const knownStates = ["AVAILABLE", "NOT STARTED", "IN PROGRESS", "PASSED", "NEEDS WORK", "STALE", "LOCKED", "LINUX/WSL2 REQUIRED"];
    state.progressFresh = state.tasks.length > 0 && state.tasks.every((task) => knownStates.includes(task.state));
    const selected = state.task && state.tasks.find((task) => task.id === state.task.id);
    if (state.task) {
      const selectedState = selected ? selected.state : "UNKNOWN";
      if (!selected) state.progressFresh = false;
      state.task.state = selectedState;
      if (selected) state.task.execution_available = selected.execution_available;
      ui.taskState.textContent = stateLabel(selectedState);
      ui.taskState.dataset.state = selectedState;
      ui.startButton.disabled = state.busy || !state.progressFresh || selectedState === "LOCKED";
    }
    updateProgressSummary();
    renderCourseList();
    const next = state.tasks.find((task) => ["IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state))
      || state.tasks.find((task) => task.state === "AVAILABLE");
    ui.continueButton.disabled = state.busy || !state.progressFresh || !next;
    ui.continueButton.textContent = !state.progressFresh ? "课程状态暂不可用" : next ? "继续学习  →" :
      state.tasks.every((task) => task.state === "PASSED") ? "课程已完成" :
      state.tasks.every((task) => task.state === "PASSED" || environmentBlocked(task)) ?
        "本机课程已完成 · 后续需要 WSL2" : "暂无可开始课程";
    updateWorkflowGuidance();
    updateResumeAction();
    enhanceGuideCourses();
    if (!state.progressFresh) toast("课程进度暂不可用，请重试刷新。");
    return state.progressFresh;
  }

  function updateWorkflowGuidance() {
    // Keep feedback on the control the learner actually clicked, including
    // the autosave and source-loading phases before the operation starts.
    if (state.busy && ui.workflowAction.dataset.pending === "true") {
      ui.workflowAction.hidden = false;
      ui.workflowAction.disabled = true;
      ui.workflowHelp.textContent = state.busyLabel || "正在处理当前课程，请等待结果。";
      return;
    }
    ui.workflowAction.hidden = true;
    ui.workflowAction.disabled = state.busy;
    delete ui.workflowAction.dataset.action;
    delete ui.workflowAction.dataset.taskId;
    const offer = (text, label, action, taskId) => {
      ui.workflowHelp.textContent = text;
      if (!label) return;
      ui.workflowAction.textContent = label;
      ui.workflowAction.dataset.action = action;
      if (taskId) ui.workflowAction.dataset.taskId = taskId;
      ui.workflowAction.hidden = false;
    };
    if (!state.task) return offer("选择课程后，这里会显示当前可以进行的下一步。");
    if (state.busy) return offer("正在处理当前课程，请等待结果后继续操作。");
    if (!state.progressFresh) {
      return offer("课程状态需要刷新，刷新后再继续下一课。", "刷新课程状态", "refresh");
    }
    if (environmentBlocked(state.task)) {
      return offer("本课需要 Linux / WSL2。可打开已准备的 Ubuntu 工作台；两边的验收记录独立，不自动迁移 PASS。", "进入 WSL2 学习", "handoff");
    }
    if (state.task.state === "LOCKED") {
      const prerequisite = (state.task.prerequisites || []).map((id) => state.tasks.find((task) => task.id === id))
        .find((task) => task && task.state !== "PASSED");
      return offer("先完成前置课程的当前源码验收；现在可以阅读本课教材和提示。",
        prerequisite ? "前往：" + courseTitle(prerequisite).replace(/^W\d+\s*[·:]\s*/, "") : "刷新课程状态",
        prerequisite ? "task" : "refresh", prerequisite && prerequisite.id);
    }
    if (state.workspaceError) {
      return offer("课程讲解仍可阅读。工作区暂未加载：" + state.workspaceError,
        "重试打开工作区", "workspace");
    }
    if (!state.workspaceReady) {
      return offer("点击开始学习创建代码工作区，再按“从这里开始”阅读并修改源文件。", "开始学习", "start");
    }
    if (state.dirtyFiles.size) return offer("代码有修改，保存并构建后继续。", "保存并构建", "build");
    const result = state.lastAction;
    if (result && result.taskId === state.task.id && !result.passed) {
      return offer(result.action === "run"
        ? "运行未完成。尚未构建或代码已变化时，请先构建；其他错误可结合输出与“故障排查”继续定位。"
        : "本次操作未通过。先看下方第一条错误，再结合本课故障排查和渐进式提示修改代码。",
      result.action === "run" ? "构建当前代码" : "查看故障排查", result.action === "run" ? "build" : "troubleshooting");
    }
    if (state.task.state === "PASSED") {
      const index = state.tasks.findIndex((task) => task.id === state.task.id);
      const following = state.tasks.slice(index + 1).concat(state.tasks.slice(0, Math.max(index, 0)));
      const next = following.find((task) => ["AVAILABLE", "IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state));
      return offer(next ? "当前源码已通过正式验收，可以继续学习下一课。" : "当前源码已通过正式验收，可以回顾教材或选择其他课程。",
        next && "继续：" + courseTitle(next).replace(/^W\d+\s*[·:]\s*/, ""), "task", next && next.id);
    }
    if (result && result.taskId === state.task.id && result.passed) {
      if (result.action === "build") return offer("构建成功。接着运行程序或课程测试，查看实际结果。", "运行 / 测试", "run");
      if (result.action === "run") return offer("运行完成。确认结果符合教材要求后，再执行正式验收。", "正式验收", "check");
    }
    if (state.task.state === "STALE") return offer("源码已修改，之前的通过记录仍保留；请构建当前代码并重新正式验收。", "构建当前代码", "build");
    if (state.task.state === "NEEDS WORK") return offer("最近一次验收未通过。按本课故障排查修改源文件，再重新构建、运行和验收。", "查看故障排查", "troubleshooting");
    offer("按教材修改源文件，完成后构建当前代码。", "构建当前代码", "build");
  }

  async function followWorkflowAction() {
    if (state.busy) return;
    const action = ui.workflowAction.dataset.action;
    if (action === "task") return selectTask(ui.workflowAction.dataset.taskId);
    if (action === "start") return startTask({ currentTarget: ui.workflowAction });
    if (action === "handoff") return handoffTask(ui.workflowAction);
    if (action === "workspace") return retryWorkspaceFiles(ui.workflowAction);
    if (action === "troubleshooting") return jumpToLessonSection("Troubleshooting");
    if (["build", "run", "check"].includes(action)) return runWorkspaceAction(action, ui.workflowAction);
    if (action === "refresh") {
      try {
        if (await loadTasks() && state.task && !ui.historyPanel.hidden) await loadTaskHistory(state.task.id);
      } catch (error) { toast("课程状态刷新失败：" + error.message); }
    }
  }

  function isReadOnlyFile(path) {
    const file = state.files.find((item) => item.path === path);
    return !file || file.read_only === true;
  }

  function enhanceLessonFiles(container) {
    for (const code of container.querySelectorAll("code")) {
      if (code.closest("pre, button")) continue;
      const file = state.files.find((item) => item.path === code.textContent.trim());
      if (!file) continue;
      const button = document.createElement("button");
      button.type = "button";
      button.className = "lesson-file-button";
      button.dataset.filePath = file.path;
      button.title = (file.read_only ? "查看只读参考：" : "打开源文件：") + file.path;
      button.disabled = state.busy || !state.workspaceReady;
      code.replaceWith(button);
      button.append(code);
      button.addEventListener("click", () => loadFile(file.path));
    }
  }

  function closeHint(restoreFocus = true) {
    state.hintRequest += 1;
    state.hintDocument = null;
    ui.hintPanel.hidden = true;
    ui.lesson.hidden = false;
    updateLessonLanguageStatus();
    document.querySelectorAll("[data-hint]").forEach((button) => button.setAttribute("aria-pressed", "false"));
    if (restoreFocus) ui.lesson.focus({ preventScroll: true });
  }

  function jumpToLessonSection(section) {
    closeHint(false);
    const translated = { "Start here": "从这里开始", "Troubleshooting": "故障排查", "Acceptance": "验收要求" }[section];
    const heading = Array.from(ui.lesson.querySelectorAll("h2"))
      .find((item) => [section, translated].includes(item.textContent.trim()));
    if (!heading) return;
    const offset = heading.getBoundingClientRect().top - ui.lesson.getBoundingClientRect().top;
    ui.lesson.scrollTo({ top: ui.lesson.scrollTop + offset - 12, behavior: "smooth" });
    heading.setAttribute("tabindex", "-1");
    heading.focus({ preventScroll: true });
  }

  function guideStatus(message) {
    ui.guideStatus.hidden = !message;
    ui.guideStatus.textContent = message;
  }

  function guideHeading(section) {
    return Array.from(ui.guideContent.querySelectorAll("h2"))
      .find((heading) => heading.textContent.trim().startsWith(section));
  }

  function updateGuideSections() {
    ui.guideSections.querySelectorAll("[data-guide-section]").forEach((button) => {
      button.disabled = !guideHeading(button.dataset.guideSection);
    });
  }

  function enhanceGuideCourses() {
    for (const strong of ui.guideContent.querySelectorAll("li > strong:first-child")) {
      if (strong.parentElement.firstChild !== strong) continue;
      const label = strong.textContent.trim();
      const match = label.match(/^(w\d+\.[a-z0-9-]+)(?:\s+·\s+.+)?$/);
      const task = match && state.tasks.find((item) => item.id === match[1]);
      if (!task) continue;
      const button = document.createElement("button");
      button.type = "button";
      button.className = "guide-task-button";
      button.dataset.taskId = task.id;
      button.textContent = label;
      button.title = "打开课程讲解：" + courseTitle(task);
      button.addEventListener("click", () => selectGuideTask(task.id));
      strong.replaceWith(button);
    }
  }

  async function loadLearningGuide() {
    if (state.guideMarkdown !== null) return true;
    if (state.guideRequest) return state.guideRequest;
    guideStatus("正在读取中文学习指南…");
    ui.guideRetry.hidden = true;
    ui.guideContent.hidden = true;
    ui.guideContent.setAttribute("aria-busy", "true");
    state.guideRequest = (async () => {
      try {
        const data = await api("/api/v1/learning-guide");
        if (typeof data.markdown !== "string") throw new Error("指南内容不可用。");
        state.guideMarkdown = data.markdown;
        ui.guideContent.innerHTML = renderMarkdown(data.markdown);
        enhanceGuideCourses();
        updateGuideSections();
        ui.guideContent.hidden = false;
        guideStatus("");
        return true;
      } catch (error) {
        guideStatus("指南读取失败，请重试：" + error.message);
        ui.guideRetry.hidden = false;
        return false;
      } finally {
        ui.guideContent.setAttribute("aria-busy", "false");
        state.guideRequest = null;
      }
    })();
    return state.guideRequest;
  }

  async function openLearningGuide(opener) {
    if (!ui.guideDialog.open) {
      state.guideOpener = opener || ui.guideButton;
      state.guideSession += 1;
      if (state.guideMarkdown !== null) guideStatus("");
      ui.guideDialog.showModal();
      document.body.classList.add("guide-open");
      ui.guideClose.focus({ preventScroll: true });
    }
    const session = state.guideSession;
    const loaded = await loadLearningGuide();
    if (loaded && ui.guideDialog.open && session === state.guideSession && state.guideOpener === ui.welcomeGuide) {
      jumpToGuideSection("第一次使用", { focus: false, behavior: "auto" });
    }
    return loaded;
  }

  function closeLearningGuide(courseSelected = false) {
    if (!ui.guideDialog.open) return;
    const opener = state.guideOpener === ui.welcomeGuide && ui.welcome.hidden ? ui.guideButton : state.guideOpener;
    const focusTarget = courseSelected ? ui.taskTitle : opener || ui.guideButton;
    state.guideOpener = null;
    state.guideSession += 1;
    ui.guideDialog.close();
    document.body.classList.remove("guide-open");
    focusTarget.focus({ preventScroll: !courseSelected });
  }

  function jumpToGuideSection(section, options = {}) {
    const heading = guideHeading(section);
    if (!heading) return;
    const offset = heading.getBoundingClientRect().top - ui.guideContent.getBoundingClientRect().top;
    ui.guideContent.scrollTo({ top: ui.guideContent.scrollTop + offset - 12, behavior: options.behavior || "smooth" });
    heading.setAttribute("tabindex", "-1");
    if (options.focus !== false) heading.focus({ preventScroll: true });
  }

  async function selectGuideTask(taskId) {
    if (!ui.guideDialog.open) return false;
    const session = state.guideSession;
    const wasBusy = state.busy;
    guideStatus("正在打开课程讲解…");
    const selected = await selectTask(taskId, { focusEditor: false });
    if (!ui.guideDialog.open || session !== state.guideSession) return selected;
    if (selected) closeLearningGuide(true);
    else guideStatus(wasBusy ? "当前课程正在处理，请等待完成后再打开课程。" : state.dirtyFiles.size
      ? "当前代码未能保存，课程尚未切换。关闭指南后可检查保存状态，再重试。"
      : "课程暂未打开，请稍后重试。当前学习内容已保留。");
    return selected;
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

  function editorTheme() {
    return window.matchMedia?.("(prefers-color-scheme: dark)")?.matches ? "vs-dark" : "vs";
  }

  async function ensureEditorReady() {
    if (state.editorReady) return state.editorReady;
    state.editorReady = new Promise((resolve) => {
      let settled = false;
      let attempt = 0;
      const settle = (ready) => {
        if (settled) return;
        settled = true;
        clearTimeout(deadline);
        state.editorFallback = null;
        ui.editorLoading.hidden = true;
        if (!ready) fallbackToTextarea();
        resolve(ready);
      };
      const deadline = setTimeout(() => settle(false), 8000);
      state.editorFallback = () => settle(false);
      ui.editorLoading.hidden = false;
      ui.editorEngineStatus.textContent = "增强编辑器加载中…";

      const configureAndLoad = (root, next, isCurrent) => {
        if (!isCurrent()) return;
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
            if (!isCurrent()) return;
            try {
              state.monaco = window.monaco;
              state.editorInstance = state.monaco.editor.create(ui.monacoContainer, {
                value: "",
                language: "cpp",
                theme: editorTheme(),
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
              settle(true);
            } catch (error) {
              console.warn("Monaco editor initialization failed:", error);
              settle(false);
            }
          }, () => next());
        } catch (error) {
          console.warn("Monaco CDN configuration failed:", error);
          next();
        }
      };

      const tryLoad = (index) => {
        if (settled) return;
        if (index >= MONACO_ROOTS.length) {
          settle(false);
          return;
        }
        const currentAttempt = ++attempt;
        const isCurrent = () => !settled && currentAttempt === attempt;
        const root = MONACO_ROOTS[index];
        const next = () => { if (isCurrent()) tryLoad(index + 1); };
        if (window.require && typeof window.require.config === "function") {
          configureAndLoad(root, next, isCurrent);
          return;
        }
        const loader = document.createElement("script");
        loader.src = root + "/vs/loader.js";
        loader.crossOrigin = "anonymous";
        loader.onload = () => configureAndLoad(root, next, isCurrent);
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
    state.activeFile = path;
    state.settingEditorValue = true;
    try {
      if (state.editorInstance && state.monaco) {
        let model = state.models.get(path);
        if (!model) {
          model = state.monaco.editor.createModel(value, languageForPath(path), modelUri(path));
          state.models.set(path, model);
        } else if (!state.dirtyFiles.has(path)) {
          model.setValue(value);
        }
        state.editorInstance.setModel(model);
        state.editorInstance.updateOptions({ readOnly: state.busy || isReadOnlyFile(path) });
      } else {
        ui.editor.value = value;
        ui.editor.disabled = false;
        ui.editor.readOnly = state.busy || isReadOnlyFile(path);
      }
    } finally { state.settingEditorValue = false; }
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
      button.disabled = state.busy || !state.workspaceReady;
      if (file.read_only) {
        button.setAttribute("aria-label", file.path + "，只读参考");
        const reference = document.createElement("span");
        reference.className = "read-only-label";
        reference.textContent = "只读";
        button.append(reference);
      }
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
    ui.saveState.textContent = dirty ? "有未保存的修改" :
      state.activeFile && isReadOnlyFile(state.activeFile) ? "课程参考文件 · 只读" : "已保存到本地工作区";
    ui.saveButton.disabled = !dirty || state.busy || !state.workspaceReady;
    renderFileTabs();
    updateWorkflowGuidance();
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
    if (!state.activeFile || state.settingEditorValue || state.busy || isReadOnlyFile(state.activeFile)) return;
    state.lastAction = null;
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

  function saveFile(path, options = {}) {
    if (!state.task || !path || !state.workspaceReady || isReadOnlyFile(path)) return Promise.resolve(false);
    const key = state.task.id + ":" + path;
    if (state.savingFiles.has(key)) return state.savingFiles.get(key);
    const pending = saveFileContent(path, options).finally(() => state.savingFiles.delete(key));
    state.savingFiles.set(key, pending);
    return pending;
  }

  async function saveFileContent(path, options = {}) {
    if (!state.task || !path || !state.workspaceReady || isReadOnlyFile(path)) return false;
    const taskId = state.task.id;
    const wasPassed = state.task.state === "PASSED";
    const hadAcceptance = wasPassed || state.task.state === "STALE";
    const content = editorValueFor(path);
    const savedVersion = state.fileVersions.get(path) || 0;
    const encodedPath = path.split("/").map(encodeURIComponent).join("/");
    try {
      await api("/api/v1/workspaces/" + encodeURIComponent(taskId) + "/files/" + encodedPath, {
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
      let progressRefreshFailed = false;
      if (hadAcceptance) {
        if (wasPassed) updateTaskState(taskId, "STALE");
        try {
          progressRefreshFailed = !await loadTasks();
          if (!progressRefreshFailed && !ui.historyPanel.hidden && state.task && state.task.id === taskId) await loadTaskHistory(taskId);
        } catch (error) {
          progressRefreshFailed = true;
          toast("代码已保存，课程状态暂未刷新：" + error.message);
        }
      }
      updateDirtyState();
      if (!options.silent && !progressRefreshFailed) toast("代码已保存。");
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
    if (!options.silent && savedCount && state.progressFresh) toast("所有修改均已保存。");
    updateDirtyState();
    return true;
  }

  async function loadFile(path, options = {}) {
    if (!state.task || !state.workspaceReady || !state.files.some((file) => file.path === path)) return false;
    const taskId = state.task.id;
    const request = ++state.fileRequest;
    if (state.activeFile && state.dirtyFiles.has(state.activeFile)) {
      const saved = await saveFile(state.activeFile, { silent: true });
      if (!saved) return false;
    }
    await ensureEditorReady();
    if (!state.task || state.task.id !== taskId || request !== state.fileRequest) return false;
    let content = state.fileContents.get(path);
    if (content === undefined) {
      const encodedPath = path.split("/").map(encodeURIComponent).join("/");
      try {
        const data = await api("/api/v1/workspaces/" + encodeURIComponent(taskId) + "/files/" + encodedPath);
        if (!state.task || state.task.id !== taskId || request !== state.fileRequest) return false;
        content = data.content;
        state.fileContents.set(path, content);
      } catch (error) {
        toast(error.message);
        return false;
      }
    }
    setEditorValue(path, content);
    ui.saveButton.disabled = false;
    ui.buildButton.disabled = false;
    ui.runButton.disabled = false;
    ui.checkButton.disabled = false;
    updateDirtyState();
    renderFileTabs();
    if (options.focusEditor !== false) {
      if (state.editorInstance) state.editorInstance.focus();
      else ui.editor.focus();
    }
    rememberLearningPosition();
    if (!state.navigatingTask) writeLearningLocation();
    return true;
  }

  async function loadWorkspaceFiles(preferredFile = state.preferredFile, options = {}) {
    if (!state.task) return false;
    ui.workspaceState.textContent = "正在加载工作区…";
    const data = await api("/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/files");
    state.files = data.files || [];
    state.workspaceReady = true;
    state.workspaceError = "";
    updateApplicationGuidance();
    if (state.task && state.task.service_profile) {
      updateServiceSnapshot(state.serviceSnapshot || { state: "STOPPED", port: null, logs: "" });
    }
    const hasApplicationProfile = Boolean(state.task && state.task.runtime_profile);
    ui.appBuildButton.disabled = !hasApplicationProfile || state.busy;
    ui.appExecuteButton.disabled = !hasApplicationProfile || state.busy;
    renderFileTabs();
    enhanceLessonFiles(ui.lesson);
    enhanceLessonFiles(ui.hint);
    if (!state.files.length) {
      ui.editor.disabled = true;
      ui.saveButton.disabled = true;
      ui.buildButton.disabled = true;
      ui.runButton.disabled = true;
      ui.checkButton.disabled = true;
      throw new Error("工作区暂未提供可读文件，请重试。");
    }
    const preferred = state.files.find((file) => file.path === preferredFile);
    const active = state.files.find((file) => file.path === state.activeFile);
    const opened = await loadFile((preferred || active || state.files[0]).path, options);
    if (!opened) throw new Error("源文件暂未加载，请重试打开工作区。");
    if (preferredFile && !preferred) toast("上次或链接中的文件不可用，已打开本课可用文件。");
    state.preferredFile = state.activeFile;
    await refreshServiceStatus(true);
    ui.workspaceState.textContent = "工作区已就绪";
    return true;
  }

  function showWorkspaceError(error) {
    state.workspaceReady = false;
    state.workspaceError = error.message;
    ui.workspaceState.textContent = "工作区暂未加载";
    toast("工作区暂未加载，可重试打开；课程讲解仍可阅读。");
  }

  async function retryWorkspaceFiles(triggerButton = ui.workflowAction) {
    if (!state.task || state.busy) return false;
    setBusy(true, "正在重新读取工作区…", triggerButton, "打开中…");
    try {
      await loadWorkspaceFiles();
      writeLearningLocation();
      toast("学习工作区已恢复。");
      return true;
    } catch (error) {
      showWorkspaceError(error);
      return false;
    } finally { setBusy(false); }
  }

  function clearTaskEditorState() {
    state.fileRequest += 1;
    state.historyRequest += 1;
    state.serviceRequest += 1;
    state.lastAction = null;
    closeHint(false);
    ui.historyPanel.hidden = true;
    if (state.editorInstance) state.editorInstance.setModel(null);
    for (const model of state.models.values()) model.dispose();
    state.models.clear();
    state.fileContents.clear();
    state.fileVersions.clear();
    state.dirtyFiles.clear();
    state.files = [];
    state.activeFile = "";
    state.workspaceReady = false;
    state.workspaceError = "";
    state.preferredFile = "";
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

  async function selectTask(taskId, options = {}) {
    if (state.busy) {
      toast("当前课程正在处理，请等待完成后再切换。");
      return false;
    }
    if (!state.tasks.some((task) => task.id === taskId)) return false;
    setBusy(true);
    state.navigatingTask = true;
    try {
      if (state.dirtyFiles.size && !await saveAllDirtyFiles({ silent: true })) {
        toast("当前代码未能保存，暂不切换课程。");
        return false;
      }
      const task = await api("/api/v1/tasks/" + encodeURIComponent(taskId));
      if (state.task && state.task.service_profile && state.serviceSnapshot &&
          ["STARTING", "RUNNING"].includes(state.serviceSnapshot.state)) {
        await stopNetworkService(true, true);
      }
      state.selectedTaskId = taskId;
      state.serviceClients = [];
      state.serviceMessages = [];
      state.task = null;
      clearTaskEditorState();
      ui.welcome.hidden = true;
      ui.workbench.hidden = false;
      ui.lesson.innerHTML = '<div class="loading">正在读取课程讲解…</div>';
      resetConsole();
      renderCourseList();
      state.task = task;
      state.preferredFile = options.filePath === undefined ? learningPosition().files[taskId] || "" : options.filePath;
      renderApplicationRunner();
      renderServiceRunner();
      await refreshServiceStatus(true);
      ui.taskBreadcrumb.textContent = task.id;
      ui.taskTitle.textContent = courseTitle(task);
      const prerequisiteText = task.prerequisites && task.prerequisites.length
        ? "前置任务：" + task.prerequisites.join("、")
        : "无前置任务";
      ui.taskSubtitle.textContent =
        "W" + task.week + " · " + task.source_files.length + " 个源文件 · " +
        (task.acceptance_kind === "exact_stdout" ? "单文件 C++ 程序 · 构建 / 运行 / 验收" : "CMake / GoogleTest 工程 · 课程测试 / 正式验收") +
        " · " + prerequisiteText + (task.state_reason ? " · " + task.state_reason : "");
      ui.buildButton.textContent = task.acceptance_kind === "exact_stdout" ? "构建" : "构建测试工程";
      ui.runButton.textContent = task.acceptance_kind === "exact_stdout" ? "运行程序" : "运行课程测试";
      ui.taskState.textContent = stateLabel(task.state);
      ui.taskState.dataset.state = task.state;
      renderLesson();
      const locked = task.state === "LOCKED";
      const environmentRequired = environmentBlocked(task);
      ui.startButton.disabled = locked;
      ui.startButton.textContent = environmentRequired ? "进入 WSL2 学习  →" : locked ? "完成前置任务后解锁" :
        task.state === "PASSED" ? "打开学习工作区  →" : "开始 / 继续学习  →";
      if (["IN PROGRESS", "NEEDS WORK", "STALE", "PASSED"].includes(task.state)) {
        try { await loadWorkspaceFiles(state.preferredFile, { focusEditor: options.focusEditor }); }
        catch (error) { showWorkspaceError(error); }
      }
      rememberLearningPosition();
      writeLearningLocation(options.historyMode || "push");
      return true;
    } catch (error) {
      toast("课程暂未打开：" + error.message);
      return false;
    } finally {
      state.navigatingTask = false;
      setBusy(false);
    }
  }

  async function handoffTask(triggerButton) {
    if (!state.task || state.busy || !environmentBlocked(state.task)) return;
    const taskId = state.task.id;
    setBusy(true, "正在检查已准备的 WSL2 环境…", triggerButton, "连接 WSL2…");
    try {
      const result = await api("/api/v1/tasks/" + encodeURIComponent(taskId) + "/handoff",
        { method: "POST", body: {} });
      const target = new URL(result.url);
      if (target.protocol !== "http:" || target.hostname !== "127.0.0.1" ||
          !["8766", "8767"].includes(target.port) || target.pathname !== "/" ||
          target.search || target.username || target.password ||
          new URLSearchParams(target.hash.slice(1)).get("task") !== taskId ||
          result.execution_environment !== "linux" || result.progress_transferred !== false) {
        throw new Error("未获得有效的 Linux 工作台地址。");
      }
      toast("已连接 " + result.distribution + "。Linux 验收记录独立，可使用浏览器后退返回本机。");
      window.location.assign(target.href);
    } catch (error) {
      toast(error.message);
      beginConsoleOperation("WSL2 环境切换", "正在显示环境检查结果…");
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "环境未就绪";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
    } finally {
      setBusy(false);
    }
  }

  async function startTask(event) {
    const triggerButton = event && event.currentTarget ? event.currentTarget : ui.startButton;
    if (!state.task || state.busy) return;
    if (environmentBlocked(state.task)) return handoffTask(triggerButton);
    setBusy(true, "正在准备学习工作区…", triggerButton, "准备中…");
    try {
      const result = await api("/api/v1/tasks/" + encodeURIComponent(state.task.id) + "/start", {
        method: "POST", body: {}
      });
      updateTaskState(state.task.id, state.task.state === "PASSED" ? "PASSED" : "IN PROGRESS");
      state.workspaceReady = true;
      ui.workspaceState.textContent = result.resumed ? "已恢复" : "已创建";
      try { await loadWorkspaceFiles(); }
      catch (error) { showWorkspaceError(error); return; }
      toast(result.resumed ? "已恢复上次的学习工作区。" : "学习工作区已创建。");
    } catch (error) {
      toast(error.message);
      beginConsoleOperation("工作区打开结果", "正在显示工作区错误…");
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "打开失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
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

  // Only actual diagnostic-shaped lines are matched. Ordinary user output
  // and unrecognized errors get no invented knowledge association.
  function diagnosticTopics(output) {
    const clean = String(output).replace(/\x1b\[[0-9;]*m/g, "");
    const lines = clean.split(/\r?\n/);
    const result = new Set();
    for (const line of lines) {
      if (/\b(?:undefined reference to|unresolved external symbol)\b/i.test(line) &&
          /(?:ld:|linker|\.o:|collect2:)/i.test(line)) result.add("linkage");
      if (/^CMake Error\b/i.test(line)) result.add("cmake-target");
      if (/^==\d+==ERROR: AddressSanitizer:|:\d+:\d+: runtime error:/i.test(line)) result.add("lifetime-sanitizers");
      if (!/:\d+(?::\d+)?:\s*(?:fatal )?error:/i.test(line)) continue;
      if (/read.only|const-qualified|discards qualifiers/i.test(line)) result.add("const-reference");
      if (/deleted function|deleted constructor/i.test(line)) result.add("ownership");
      if (/no matching function|conflicting types.*(?:parameter|deduc)/i.test(line)) result.add("template-deduction");
      if (/no type named.*enable_if|substitution failure/i.test(line)) result.add("type-traits");
      if (/no match for.*operator.*\+/i.test(line)) result.add("iterators");
      if (/narrowing conversion/i.test(line)) result.add("narrowing");
      if (/does not override/i.test(line)) result.add("virtual-override");
      if (/abstract type/i.test(line)) result.add("abstract-interface");
      if (/not captured/i.test(line)) result.add("lambda-capture");
      if (/not declared in this scope/i.test(line)) result.add("scope");
      if (/static assertion failed/i.test(line)) {
        result.add(/missing explicit specialization/i.test(line) ? "template-specialization" : "constexpr");
      }
    }
    return [...result];
  }

  const knowledgeState = { topics: null, active: "", running: false, request: null, session: 0, opener: null };
  function knowledgeMatches(query) {
    const words = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
    return (knowledgeState.topics || []).filter((topic) => {
      const haystack = [topic.title, topic.terms, topic.id, topic.task].join(" ").toLocaleLowerCase();
      return words.every((word) => haystack.includes(word));
    });
  }

  function renderKnowledge() {
    const topics = knowledgeMatches($("knowledge-search").value);
    const results = $("knowledge-results");
    results.replaceChildren();
    for (const topic of topics) {
      const control = document.createElement("button");
      control.type = "button";
      control.className = "knowledge-topic";
      control.dataset.topic = topic.id;
      control.setAttribute("aria-current", String(topic.id === knowledgeState.active));
      control.textContent = (topic.standard > 17 ? "C++" + topic.standard + " · " : "") +
        topic.title + (topic.phase ? "" : "（阅读）");
      control.addEventListener("click", () => selectKnowledge(topic.id));
      results.append(control);
    }
    $("knowledge-count").textContent = topics.length + " 个知识点";
    const topic = (knowledgeState.topics || []).find((item) => item.id === knowledgeState.active);
    $("knowledge-detail").hidden = !topic;
    if (!topic) return;
    $("knowledge-topic-title").textContent = topic.title;
    $("knowledge-explanation").textContent = topic.explanation;
    $("knowledge-prediction").textContent = topic.prediction;
    $("knowledge-transfer").textContent = topic.transfer;
    $("knowledge-lesson").textContent = "打开课程：" + topic.task + " · " +
      (state.lessonLanguage === "en" ? topic.en : topic.zh);
    $("knowledge-lesson").hidden = !topic.course_available;
    $("knowledge-experiment").hidden = !topic.phase;
    const profile = $("knowledge-profile");
    profile.hidden = !(topic.standard > 17) && topic.execution_available !== false;
    if (topic.execution_available === false) {
      profile.textContent = topic.environment_reason || "本环境仅支持阅读，实验执行尚待验证。";
    } else if (!profile.hidden) {
      profile.textContent = "C++" + topic.standard + " · " +
        (topic.compiler_profile || "独立 GCC 13 配置") + " · " +
        (topic.toolchain_present ? "已检测编译器文件，运行时核验版本及特性宏"
          : "缺少 /usr/bin/g++-13；Ubuntu 24.04 可手动安装 g++-13 后刷新网页。原 C++17 课程不受影响。");
    }
    if (topic.phase) {
      $("knowledge-bad").textContent = topic.bad;
      $("knowledge-good").textContent = topic.good;
    }
    const unavailable = topic.execution_available === false || (topic.standard > 17 && !topic.toolchain_present);
    $("knowledge-failure").disabled = knowledgeState.running || !state.csrfToken || unavailable;
    $("knowledge-repair").disabled = knowledgeState.running || !state.csrfToken || unavailable;
  }

  function selectKnowledge(id) {
    knowledgeState.active = id;
    $("knowledge-output").textContent = "";
    $("knowledge-status").textContent = knowledgeState.running ? "上一实验尚未结束；完成后可再次运行。" : "";
    renderKnowledge();
  }

  async function openKnowledge(opener, topicId = "") {
    const dialog = $("knowledge-dialog");
    if (!dialog.open) {
      knowledgeState.opener = opener;
      knowledgeState.session += 1;
      dialog.showModal();
    }
    $("knowledge-search").value = "";
    knowledgeState.active = topicId;
    $("knowledge-output").textContent = "";
    $("knowledge-status").textContent = "正在读取知识点…";
    const session = knowledgeState.session;
    try {
      if (!knowledgeState.topics) {
        if (!knowledgeState.request) {
          knowledgeState.request = api("/api/v1/knowledge").then((data) => {
            knowledgeState.topics = data.topics;
          }).finally(() => { knowledgeState.request = null; });
        }
        await knowledgeState.request;
      }
      if (!dialog.open || session !== knowledgeState.session) return;
      if (!knowledgeState.active) knowledgeState.active = knowledgeState.topics[0]?.id || "";
      $("knowledge-status").textContent = knowledgeState.running ? "实验正在运行。" : "";
      renderKnowledge();
      $("knowledge-search").focus({ preventScroll: true });
    } catch (error) {
      if (dialog.open && session === knowledgeState.session) $("knowledge-status").textContent = "知识点暂未读取：" + error.message;
    }
  }

  function closeKnowledge() {
    const dialog = $("knowledge-dialog");
    if (!dialog.open) return;
    knowledgeState.session += 1;
    dialog.close();
    const opener = knowledgeState.opener;
    knowledgeState.opener = null;
    if (opener?.isConnected && !opener.hidden) opener.focus({ preventScroll: true });
    else $("knowledge-open").focus({ preventScroll: true });
  }

  async function runKnowledge(variant) {
    if (knowledgeState.running || !state.csrfToken) return;
    const topic = (knowledgeState.topics || []).find((item) => item.id === knowledgeState.active);
    if (!topic?.phase || topic.execution_available === false || (topic.standard > 17 && !topic.toolchain_present)) return;
    knowledgeState.running = true;
    const session = knowledgeState.session;
    $("knowledge-status").textContent = "正在本机运行固定实验…";
    $("knowledge-output").textContent = "";
    renderKnowledge();
    try {
      const data = await api("/api/v1/knowledge/" + encodeURIComponent(topic.id) + "/run", {
        method: "POST", body: { variant }
      });
      if (!$("knowledge-dialog").open || session !== knowledgeState.session || topic.id !== knowledgeState.active) return;
      $("knowledge-output").textContent = data.steps.map((step) =>
        "$ " + step.command.join(" ") + "\n[" + step.phase + "] exit=" + step.exit_code +
        (step.timed_out ? " · timeout" : "") + "\n" + step.output).join("\n\n");
      const modernVersion = data.standard > 17 && data.compiler_version
        ? " · GCC " + data.compiler_version : "";
      $("knowledge-status").textContent = data.status === "TOOLCHAIN_UNAVAILABLE"
        ? "现代工具链不可用：" + (topic.compiler_profile || "必须是 GCC 13.x") + "，不回退 C++17。"
        : data.status === "FEATURE_UNAVAILABLE"
        ? "现代特性检查未通过；不计为预期失败。"
        : data.observed
        ? (variant === "failure" ? "已观察到预期失败。" : "修复程序实际运行成功。") + " 不改变课程验收。" + modernVersion
        : "未观察到预期结果，请查看真实输出。";
    } catch (error) {
      if ($("knowledge-dialog").open && session === knowledgeState.session && topic.id === knowledgeState.active)
        $("knowledge-status").textContent = "实验未完成：" + error.message;
    } finally {
      knowledgeState.running = false;
      if ($("knowledge-dialog").open) renderKnowledge();
    }
  }

  async function openKnowledgeLesson() {
    const topic = (knowledgeState.topics || []).find((item) => item.id === knowledgeState.active);
    if (!topic?.course_available || state.busy) {
      $("knowledge-status").textContent = "当前课程正在处理或教材暂不可用。";
      return;
    }
    const session = knowledgeState.session;
    if (state.task?.id !== topic.task && !await selectTask(topic.task, { focusEditor: false })) {
      if ($("knowledge-dialog").open && session === knowledgeState.session)
        $("knowledge-status").textContent = "课程未切换；请先检查当前保存状态。";
      return;
    }
    if (!$("knowledge-dialog").open || session !== knowledgeState.session) return;
    closeKnowledge();
    closeHint(false);
    const heading = Array.from(ui.lesson.querySelectorAll("h2,h3"))
      .find((item) => [topic.zh, topic.en].includes(item.textContent.trim()));
    if (heading) {
      const offset = heading.getBoundingClientRect().top - ui.lesson.getBoundingClientRect().top;
      ui.lesson.scrollTo({ top: ui.lesson.scrollTop + offset - 12, behavior: "auto" });
      heading.setAttribute("tabindex", "-1");
      heading.focus({ preventScroll: true });
    } else ui.taskTitle.focus();
  }

  const modernState = { projects: [], active: "", record: null, busy: false };
  function modernDirty() {
    return !!modernState.record && $("modern-source").value !== modernState.record.source;
  }
  function modernStatus(text, status = "") {
    $("modern-status").textContent = text;
    $("modern-status").dataset.status = status;
  }
  function renderModern() {
    const meta = modernState.projects.find(item => item.id === modernState.active);
    const record = modernState.record;
    $("modern-choice").disabled = modernState.busy || !meta;
    $("modern-start").disabled = modernState.busy || !meta || !!record || !state.csrfToken || meta.execution_available === false;
    $("modern-close").disabled = modernState.busy;
    $("modern-start").textContent = meta?.execution_available === false ? "原生执行尚待验证" : record ? "项目已打开" : "打开学习项目";
    $("modern-profile").textContent = meta?.execution_available === false ? meta.environment_reason : meta ? "C++" + meta.standard + " · " +
      (meta.compiler_profile === "linux-gcc13" || !meta.compiler_profile ? "独立 GCC 13" : meta.compiler_profile) + " · " +
      meta.count + " 项固定 GoogleTest · " + (meta.toolchain_present ?
        "运行时核验工具链" : "缺少 g++-13；Ubuntu 24.04 可安装 g++-13、cmake 和 libgtest-dev 后刷新网页") : "";
    $("modern-task").textContent = meta ? (state.lessonLanguage === "en" ? meta.en : meta.zh) : "";
    $("modern-workspace").hidden = !record;
    $("modern-source").readOnly = modernState.busy;
    $("modern-save").disabled = modernState.busy || !record || !modernDirty() || !state.csrfToken;
    $("modern-check").disabled = modernState.busy || !record || modernDirty() || !meta?.toolchain_present || !state.csrfToken;
    if (record) {
      $("modern-current").textContent = modernDirty() ? "未保存修改，旧结果不适用" :
        ({ PASS: "当前源码与合同检查通过", FAIL: "当前源码检查未通过",
          STALE: "源码或合同已变化，需重检", UNTESTED: "尚未检查" }[record.state] || "需重新检查");
      $("modern-current").dataset.state = modernDirty() ? "DIRTY" : record.state;
    }
  }
  function installModern(record) {
    modernState.record = record;
    $("modern-source").value = record.source;
    $("modern-contract").textContent = record.contract;
    $("modern-path").textContent = record.path;
    renderModern();
  }
  function allowModernLeave() {
    return !modernDirty() || window.confirm("现代项目有未保存修改。确定放弃当前草稿？");
  }
  async function openModern() {
    if (modernState.busy) return;
    closeKnowledge();
    const dialog = $("modern-dialog");
    if (!dialog.open) dialog.showModal();
    modernState.busy = true;
    modernState.record = null;
    modernState.projects = [];
    modernState.active = "";
    $("modern-choice").replaceChildren();
    $("modern-output").textContent = "";
    modernStatus("正在读取独立项目…", "RUNNING");
    renderModern();
    try {
      const data = await api("/api/v1/modern-projects");
      modernState.projects = data.projects;
      for (const item of data.projects) {
        const option = document.createElement("option");
        option.value = item.id;
        option.textContent = item.title;
        $("modern-choice").append(option);
      }
      modernState.active = data.projects[0]?.id || "";
      $("modern-choice").value = modernState.active;
      modernStatus("选择项目，再打开自己的学员源码。", "READY");
    } catch (error) {
      modernStatus("项目目录暂不可用：" + error.message, "ERROR");
    } finally {
      modernState.busy = false;
      renderModern();
    }
  }
  function closeModern() {
    if (modernState.busy || !allowModernLeave()) return;
    modernState.record = null;
    $("modern-dialog").close();
    $("knowledge-open").focus({ preventScroll: true });
  }
  function changeModern() {
    if (modernState.busy || !allowModernLeave()) {
      $("modern-choice").value = modernState.active;
      return;
    }
    modernState.active = $("modern-choice").value;
    modernState.record = null;
    $("modern-output").textContent = "";
    modernStatus("打开项目以继续编辑。", "READY");
    renderModern();
  }
  async function modernAction(action) {
    if (modernState.busy || !modernState.active || !state.csrfToken) return;
    const record = modernState.record;
    if (action === "start" ? !!record : !record) return;
    if (action === "save" && !modernDirty()) return;
    if (action === "check" && (modernDirty() ||
        !modernState.projects.find(item => item.id === modernState.active)?.toolchain_present)) return;
    modernState.busy = true;
    renderModern();
    modernStatus(action === "check" ? "正在编译并运行固定测试…" :
      action === "save" ? "正在保存…" : "正在打开独立工作区…", "RUNNING");
    if (action !== "save") $("modern-output").textContent = "";
    try {
      const body = action === "start" ? {} : action === "save"
        ? { source: $("modern-source").value, revision: record.revision }
        : { revision: record.revision };
      const data = await api("/api/v1/modern-projects/" + encodeURIComponent(modernState.active) + "/" + action,
        { method: "POST", body });
      if (action !== "check") {
        installModern(data);
        modernStatus(action === "save" ? "源码已保存。" : "已打开；不会覆盖已有代码。", "READY");
      } else {
        if (["PASS", "FAIL", "STALE"].includes(data.status)) record.state = data.status;
        $("modern-output").textContent = data.steps.map(step =>
          "$ " + step.command.join(" ") + "\n[" + step.phase + "] exit=" + step.exit_code +
          (step.timed_out ? " · timeout" : "") + "\n" + step.output).join("\n\n");
        const message = ({
          PASS: "独立项目 PASS · " + data.observed_tests + "/" + data.expected_tests + " 项测试通过",
          FAIL: "检查未通过；阅读真实构建或测试输出。",
          STALE: "检查期间源码或合同变化；本次结果不适用。",
          TOOLCHAIN_UNAVAILABLE: "现代工具链不可用；未完成检查。",
          FEATURE_UNAVAILABLE: "特性宏检查未通过；未完成检查。"
        })[data.status] || "未获得有效检查结果。";
        modernStatus(message, data.status);
      }
    } catch (error) {
      modernStatus("操作未完成：" + error.message + "；当前草稿保留。", "ERROR");
    } finally {
      modernState.busy = false;
      renderModern();
    }
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
      if (path && state.monaco) {
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
    const related = diagnosticTopics(output);
    for (const id of related) {
      const control = document.createElement("button");
      control.type = "button";
      control.className = "secondary-button diagnostic-knowledge";
      control.dataset.knowledgeTopic = id;
      control.textContent = "查知识与对照实验：" + ({
        linkage: "声明与链接", "const-reference": "const 与引用", ownership: "所有权与移动",
        "template-deduction": "模板与重载", iterators: "迭代器", "cmake-target": "CMake",
        "lifetime-sanitizers": "生命周期与 Sanitizer", narrowing: "列表初始化与窄化",
        "virtual-override": "虚函数与 override", "abstract-interface": "抽象类",
        "lambda-capture": "Lambda 捕获", scope: "作用域", constexpr: "编译期断言",
        "template-specialization": "类模板特化", "type-traits": "类型特征与 SFINAE"
      }[id] || id);
      control.addEventListener("click", () => openKnowledge(control, id));
      ui.diagnostics.append(control);
    }
    ui.diagnostics.hidden = diagnostics.length === 0 && related.length === 0;
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

  function setConsoleMode(mode) {
    if (!["collapsed", "normal", "large"].includes(mode)) return;
    const collapsed = mode === "collapsed";
    const large = mode === "large";
    ui.consoleDock.dataset.mode = mode;
    ui.consoleDock.dataset.collapsed = String(collapsed);
    ui.consoleDock.classList.toggle("is-collapsed", collapsed);
    ui.consoleToggle.setAttribute("aria-expanded", String(!collapsed));
    ui.consoleToggle.textContent = collapsed ? "展开终端" : "收起终端";
    ui.consoleToggle.setAttribute("aria-label", collapsed ? "展开运行终端" : "收起运行终端");
    ui.consoleSize.hidden = collapsed;
    ui.consoleSize.textContent = large ? "还原" : "放大";
    ui.consoleSize.setAttribute("aria-expanded", String(large));
    ui.consoleSize.setAttribute("aria-label", large ? "还原终端大小" : "放大终端查看完整日志");
    state.editorInstance?.layout?.();
  }

  function resetConsole() {
    ui.consoleDock.dataset.state = "idle";
    ui.consoleTitle.textContent = "运行终端";
    ui.consoleSummary.textContent = "执行构建、运行或验收时自动展开。";
    ui.consoleSummary.title = ui.consoleSummary.textContent;
    ui.console.textContent = "开始任务后，可以在这里查看构建、运行与验收输出。";
    ui.diagnostics.replaceChildren();
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "等待执行";
    ui.checkResult.className = "check-result";
    setConsoleMode("collapsed");
  }

  function beginConsoleOperation(title, progress) {
    ui.consoleDock.dataset.state = "running";
    ui.consoleTitle.textContent = title;
    ui.console.textContent = progress;
    ui.consoleSummary.textContent = progress;
    ui.consoleSummary.title = progress;
    ui.diagnostics.replaceChildren();
    ui.diagnostics.hidden = true;
    ui.checkResult.textContent = "执行中";
    ui.checkResult.className = "check-result";
    setConsoleMode("normal");
  }

  function finishConsoleOperation(passed, summary) {
    ui.consoleDock.dataset.state = passed ? "success" : "error";
    if (!summary && !passed) {
      const lines = ui.console.textContent.replace(/\x1b\[[0-9;]*m/g, "")
        .split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
      const compilerError = lines.find((line) => /:\d+(?::\d+)?:\s*(?:fatal )?error:/i.test(line));
      // Put the reason before long temporary paths so a compact phone header
      // reveals the actual problem. The full location stays in diagnostics/logs.
      const error = compilerError
        ? compilerError.replace(/^.*?:\d+(?::\d+)?:\s*(?:fatal )?error:\s*/i, "")
        : lines.find((line) => /error|failed|failure|错误|失败/i.test(line)) || lines[0];
      summary = error ? ui.checkResult.textContent + " · " + error : ui.checkResult.textContent;
    }
    ui.consoleSummary.textContent = summary || ui.checkResult.textContent;
    ui.consoleSummary.title = ui.consoleSummary.textContent;
    // A learner may collapse while an operation is running. Keep that choice
    // and announce the new outcome in the always-visible header instead.
  }

  function updateStartAction() {
    const ready = state.workspaceReady && !state.workspaceError;
    const focused = ready && document.activeElement === ui.startButton;
    ui.startButton.hidden = ready;
    ui.workspaceState.hidden = !ready && !state.workspaceError;
    if (focused) {
      if (!ui.workflowAction.hidden) ui.workflowAction.focus({ preventScroll: true });
      else ui.taskTitle.focus({ preventScroll: true });
    }
  }

  function setButtonPending(button, label) {
    if (!button) return;
    if (button.dataset.pending !== "true") {
      button.dataset.idleLabel = button.textContent.trim();
    }
    button.dataset.pending = "true";
    button.setAttribute("aria-busy", "true");
    if (label) button.textContent = label;
  }

  function clearButtonPending(button) {
    if (!button || button.dataset.pending !== "true") return;
    button.textContent = button.dataset.idleLabel || button.textContent;
    delete button.dataset.idleLabel;
    delete button.dataset.pending;
    button.setAttribute("aria-busy", "false");
  }

  function setBusy(value, saveLabel, pendingButton, pendingLabel) {
    state.busy = value;
    if (saveLabel) state.busyLabel = saveLabel;
    if (!value) state.busyLabel = "";
    if (value) {
      state.serviceRequest += 1;
      if (pendingButton) setButtonPending(pendingButton, pendingLabel || "执行中…");
    } else {
      [
        ui.startButton,
        ui.workflowAction,
        ui.saveButton,
        ui.buildButton,
        ui.runButton,
        ui.checkButton,
        ui.appBuildButton,
        ui.appExecuteButton,
        ui.serviceStartButton,
        ui.serviceRefreshButton,
        ui.serviceStopButton
      ].forEach(clearButtonPending);
    }
    ui.startButton.disabled = value || !state.progressFresh || !state.task || ["LOCKED", "UNKNOWN"].includes(state.task.state);
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
    document.querySelectorAll("#course-list .task-link").forEach((button) => {
      button.disabled = value;
    });
    document.querySelectorAll(".lesson-file-button, .file-tab").forEach((button) => {
      button.disabled = value || !state.workspaceReady;
    });
    ui.continueButton.disabled = value || !state.progressFresh || !state.tasks.some((task) => ["AVAILABLE", "IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state));
    updateResumeAction();
    const readOnly = value || !state.workspaceReady || isReadOnlyFile(state.activeFile);
    if (state.editorInstance) state.editorInstance.updateOptions({ readOnly });
    ui.editor.readOnly = readOnly;
    if (state.task && state.task.service_profile) {
      updateServiceSnapshot(state.serviceSnapshot || { state: "STOPPED", port: null, logs: "" });
    }
    if (saveLabel) ui.saveState.textContent = saveLabel;
    updateWorkflowGuidance();
    updateStartAction();
  }

  function updateApplicationGuidance() {
    if (!state.task || !state.task.runtime_profile) return;
    if (state.task.state === "LOCKED") {
      ui.appRunnerHelp.textContent = "当前课程尚未解锁。先完成前置课程的正式验收，才能构建和运行此程序。";
    } else if (!state.workspaceReady) {
      ui.appRunnerHelp.textContent = "先点击“开始学习”创建工作区，然后构建应用、选择操作并运行。";
    } else {
      ui.appRunnerHelp.textContent = "运行的是当前工作区里的学习者代码。请对照课程预期查看下方输出；错误输入可能按设计返回非零状态。运行不会改变正式验收状态。" +
        (state.task.id === "w1.calculator"
          ? "例如 divide 1 0 应输出 error: division by zero，学习者程序按设计以退出码 3 结束。"
          : "");
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

  async function refreshServiceStatus(silent = false, triggerButton = ui.serviceRefreshButton) {
    if (!state.task || !state.task.service_profile || (state.busy && !silent)) return;
    const taskId = state.task.id;
    if (!silent && triggerButton && triggerButton.dataset.pending === "true") return null;
    const request = ++state.serviceRequest;
    if (!silent) setButtonPending(triggerButton, "刷新中…");
    try {
      const snapshot = await api(
        "/api/v1/workspaces/" + encodeURIComponent(taskId) + "/service/status"
      );
      if (!state.task || state.task.id !== taskId || request !== state.serviceRequest) return null;
      updateServiceSnapshot(snapshot);
      return snapshot;
    } catch (error) {
      if (!state.task || state.task.id !== taskId || request !== state.serviceRequest) return null;
      if (!silent) toast(error.message);
      return null;
    } finally {
      if (!silent) clearButtonPending(triggerButton);
    }
  }

  async function startNetworkService(triggerButton = ui.serviceStartButton) {
    if (!state.task || !state.task.service_profile || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存代码…", triggerButton, "准备中…");
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

  async function stopNetworkService(silent = false, navigationOwnsBusy = false, triggerButton = ui.serviceStopButton) {
    if (!state.task || !state.task.service_profile || (state.busy && !navigationOwnsBusy)) return;
    try {
      setBusy(true, "正在停止网络服务…", silent ? null : triggerButton, "停止中…");
      const snapshot = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/service/stop",
        { method: "POST", body: {} }
      );
      updateServiceSnapshot(snapshot);
      if (!silent) toast("网络服务已停止，子进程组已清理。");
    } catch (error) {
      if (!silent) toast(error.message);
    } finally {
      if (!navigationOwnsBusy) setBusy(false);
    }
  }

  async function sendTcpEchoMessage(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || !state.task.service_profile || state.task.service_profile.kind !== "tcp_echo" || state.busy) return;
    const input = ui.serviceControls.querySelector("#service-echo-message");
    const message = input ? input.value : "";
    if (!message.trim()) {
      toast("请输入要发送的消息。");
      return;
    }
    setBusy(true, "正在连接 TCP Echo 服务…", triggerButton, "发送中…");
    beginConsoleOperation("TCP Echo 客户端输出", "正在连接 TCP Echo 服务…");
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
      finishConsoleOperation(result.success);
      toast(result.success ? "已收到学习者 TCP Echo 服务的真实回显。" : "TCP Echo 客户端执行失败，请查看输出和服务日志。");
    } catch (error) {
      toast(error.message);
      ui.consoleTitle.textContent = "TCP Echo 客户端输出";
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "回显失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
    } finally {
      setBusy(false);
      clearButtonPending(triggerButton);
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

  async function connectChatClient(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || !state.serviceSnapshot || state.serviceSnapshot.state !== "RUNNING" || state.busy) return;
    const input = ui.serviceControls.querySelector("#service-chat-nickname");
    const nickname = input ? input.value.trim() : "";
    if (!nickname) {
      toast("请先填写客户端昵称。");
      return;
    }
    setBusy(true, "正在连接 TCP Chat 客户端…", triggerButton, "连接中…");
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
      beginConsoleOperation("TCP Chat 连接结果", "正在显示连接错误…");
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "连接失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
    } finally {
      setBusy(false);
      clearButtonPending(triggerButton);
    }
  }

  async function sendChatMessage(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const input = ui.serviceControls.querySelector("#service-chat-message");
    const clientId = select ? select.value : "";
    const message = input ? input.value : "";
    if (!clientId || !message.trim()) {
      toast("请选择客户端并填写消息。");
      return;
    }
    setBusy(true, "正在发送 TCP Chat 消息…", triggerButton, "发送中…");
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
      clearButtonPending(triggerButton);
    }
  }

  async function pollChatMessages(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const clientId = select ? select.value : "";
    if (!clientId) {
      toast("请先连接一个 Chat 客户端。");
      return;
    }
    setBusy(true, "正在刷新 TCP Chat 消息…", triggerButton, "刷新中…");
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
      clearButtonPending(triggerButton);
    }
  }

  async function disconnectChatClient(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || state.busy) return;
    const select = ui.serviceControls.querySelector("#service-chat-client");
    const clientId = select ? select.value : "";
    const client = state.serviceClients.find((item) => item.id === clientId);
    if (!clientId || !client) {
      toast("请选择需要断开的客户端。");
      return;
    }
    setBusy(true, "正在断开 TCP Chat 客户端…", triggerButton, "断开中…");
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
      clearButtonPending(triggerButton);
    }
  }

  async function sendHttpServiceRequest(event) {
    const triggerButton = event && event.currentTarget;
    if (!state.task || state.busy) return;
    const method = ui.serviceControls.querySelector("#service-http-method")?.value || "GET";
    const target = ui.serviceControls.querySelector("#service-http-target")?.value || "/";
    const body = ui.serviceControls.querySelector("#service-http-body")?.value || "";
    setBusy(true, "正在向学习者 HTTP Server 发送请求…", triggerButton, "请求中…");
    beginConsoleOperation("HTTP 请求 / 响应", "正在向学习者 HTTP Server 发送请求…");
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
      finishConsoleOperation(result.status_code >= 200 && result.status_code < 400,
        method + " " + target + " · HTTP " + result.status_code + " " + result.reason);
    } catch (error) {
      ui.consoleTitle.textContent = "HTTP 请求 / 响应";
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "请求失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
      toast(error.message);
    } finally {
      setBusy(false);
      clearButtonPending(triggerButton);
    }
  }

  async function runApplicationBuild() {
    if (!state.task || !state.task.runtime_profile || !state.workspaceReady || state.busy) return;
    setBusy(true, "正在保存代码…", ui.appBuildButton, "准备中…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消应用构建。");
      return;
    }
    setButtonPending(ui.appBuildButton, "构建中…");
    beginConsoleOperation("应用程序构建输出", "正在构建当前任务的固定应用配置…");
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/app-build",
        { method: "POST", body: {} }
      );
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      ui.checkResult.textContent = result.success ? "应用构建成功" : "应用构建失败";
      ui.checkResult.className = "check-result " + (result.success ? "pass" : "fail");
      finishConsoleOperation(result.success);
      toast(result.success ? "应用程序构建成功；正式验收状态未改变。" : "应用构建失败，请查看编译诊断。");
    } catch (error) {
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "应用构建失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
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
    setBusy(true, "正在保存代码…", ui.appExecuteButton, "准备中…");
    const saved = await saveAllDirtyFiles({ silent: true });
    if (!saved) {
      setBusy(false);
      toast("代码保存失败，已取消程序运行。");
      return;
    }
    setButtonPending(ui.appExecuteButton, "运行中…");
    beginConsoleOperation("程序交互输出", "正在执行：" + (action.label || action.id));
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(state.task.id) + "/app-run",
        { method: "POST", body: { action: action.id, values } }
      );
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      ui.checkResult.textContent = result.success ? "程序运行成功" : "程序返回非零状态";
      ui.checkResult.className = "check-result " + (result.success ? "pass" : "fail");
      finishConsoleOperation(result.success);
      toast(result.success
        ? "程序运行完成；没有写入正式验收记录。"
        : "程序返回非零状态；请对照课程预期核对输出。没有写入正式验收记录。");
    } catch (error) {
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "程序运行失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
      toast(error.message);
    } finally {
      setBusy(false);
    }
  }

  async function runWorkspaceAction(action, triggerButton) {
    if (!state.task || !state.workspaceReady || state.busy) return;
    const taskId = state.task.id;
    const pendingButton = action === "build" ? ui.buildButton :
      action === "run" ? ui.runButton : ui.checkButton;
    const clickedButton = triggerButton || pendingButton;
    const pendingLabels = { build: "构建中…", run: "运行 / 测试中…", check: "正式验收中…" };
    setButtonPending(pendingButton, "准备中…");
    setBusy(true, "正在保存代码…", clickedButton, "准备中…");
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
    const title = action === "build" ? "构建输出" :
      action === "run" ? (state.task.acceptance_kind === "exact_stdout" ? "程序运行输出" : "测试运行输出") :
      "平台固定验收输出";
    beginConsoleOperation(title, labels[action]);
    setButtonPending(pendingButton, pendingLabels[action]);
    setButtonPending(clickedButton, pendingLabels[action]);
    setBusy(true, labels[action]);
    try {
      const result = await api(
        "/api/v1/workspaces/" + encodeURIComponent(taskId) + "/" + action,
        { method: "POST", body: {} }
      );
      if (!state.task || state.task.id !== taskId) return;
      state.lastAction = { taskId, action, passed: result.passed };
      ui.console.textContent = result.output || "(没有输出)";
      renderDiagnostics(result.output || "");
      const passed = result.passed;
      ui.checkResult.textContent = action === "check" ? (passed ? "PASS" : "未通过") :
        action === "build" ? (passed ? "构建成功" : "构建失败") :
        (passed ? "执行成功" : "执行失败");
      ui.checkResult.className = "check-result " + (passed ? "pass" : "fail");
      finishConsoleOperation(passed);
      if (action === "check") {
        updateTaskState(taskId, passed ? "PASSED" : "NEEDS WORK");
        try {
          // Refresh authoritative prerequisite states after either PASS or FAIL.
          const refreshed = await loadTasks();
          if (refreshed && !ui.historyPanel.hidden) await loadTaskHistory(taskId);
          if (refreshed && state.progressFresh) toast(passed ? "平台固定验收已通过，下一任务已更新。" : "验收未通过，请根据诊断继续修改。");
        } catch (refreshError) {
          toast("验收已记录，但课程状态刷新失败：" + refreshError.message);
        }
      } else if (action === "build") {
        toast(passed ? "构建成功。尚未记录正式验收结果。" : "构建失败，请查看编译诊断。");
      } else {
        toast(passed ? "运行 / 测试完成。正式验收需要单独执行。" : "运行 / 测试未通过，请查看输出。");
      }
    } catch (error) {
      if (!state.task || state.task.id !== taskId) return;
      state.lastAction = { taskId, action, passed: false };
      ui.console.textContent = error.message;
      ui.checkResult.textContent = "执行失败";
      ui.checkResult.className = "check-result fail";
      finishConsoleOperation(false);
      toast(error.message);
    } finally {
      setBusy(false);
      updateDirtyState();
    }
  }

  async function showHint(level) {
    if (!state.task) return;
    const taskId = state.task.id;
    const request = ++state.hintRequest;
    state.hintDocument = null;
    ui.hintTitle.textContent = "渐进式提示 " + level;
    ui.hint.innerHTML = '<div class="loading">正在读取提示…</div>';
    ui.lesson.hidden = true;
    ui.hintPanel.hidden = false;
    ui.hint.lang = "zh-CN";
    updateLessonLanguageStatus();
    document.querySelectorAll("[data-hint]").forEach((button) => {
      button.setAttribute("aria-pressed", String(Number(button.dataset.hint) === level));
    });
    try {
      const data = await api("/api/v1/tasks/" + encodeURIComponent(taskId) + "/hints/" + level);
      if (!state.task || state.task.id !== taskId || request !== state.hintRequest) return;
      state.hintDocument = data;
      renderHint();
      ui.hint.scrollTop = 0;
      ui.hint.focus({ preventScroll: true });
    } catch (error) {
      if (!state.task || state.task.id !== taskId || request !== state.hintRequest) return;
      ui.hint.textContent = "提示读取失败，请重试：" + error.message;
      toast(error.message);
    }
  }

  function bindEvents() {
    // Match Monaco to the system palette without recreating models or buffers.
    window.matchMedia?.("(prefers-color-scheme: dark)")?.addEventListener?.("change", () => {
      state.monaco?.editor?.setTheme?.(editorTheme());
    });
    ui.lessonLanguage.value = state.lessonLanguage;
    ui.lessonLanguage.addEventListener("change", () => changeLessonLanguage(ui.lessonLanguage.value));
    ui.search.addEventListener("input", renderCourseList);
    $("knowledge-open")?.addEventListener("click", () => openKnowledge($("knowledge-open")));
    $("knowledge-close")?.addEventListener("click", closeKnowledge);
    $("knowledge-dialog")?.addEventListener("cancel", (event) => { event.preventDefault(); closeKnowledge(); });
    $("knowledge-search")?.addEventListener("input", renderKnowledge);
    $("knowledge-failure")?.addEventListener("click", () => runKnowledge("failure"));
    $("knowledge-repair")?.addEventListener("click", () => runKnowledge("repair"));
    $("knowledge-lesson")?.addEventListener("click", openKnowledgeLesson);
    $("modern-open")?.addEventListener("click", openModern);
    $("modern-close")?.addEventListener("click", closeModern);
    $("modern-dialog")?.addEventListener("cancel", event => { event.preventDefault(); closeModern(); });
    $("modern-choice")?.addEventListener("change", changeModern);
    $("modern-start")?.addEventListener("click", () => modernAction("start"));
    $("modern-save")?.addEventListener("click", () => modernAction("save"));
    $("modern-check")?.addEventListener("click", () => modernAction("check"));
    $("modern-source")?.addEventListener("input", renderModern);
    window.addEventListener("beforeunload", event => {
      if (modernDirty()) { event.preventDefault(); event.returnValue = ""; }
    });
    ui.guideButton.addEventListener("click", () => openLearningGuide(ui.guideButton));
    ui.welcomeGuide.addEventListener("click", () => openLearningGuide(ui.welcomeGuide));
    ui.guideClose.addEventListener("click", () => closeLearningGuide());
    ui.guideDialog.addEventListener("cancel", (event) => {
      event.preventDefault();
      closeLearningGuide();
    });
    ui.guideRetry.addEventListener("click", loadLearningGuide);
    ui.guideSections.querySelectorAll("[data-guide-section]").forEach((button) => {
      button.addEventListener("click", () => jumpToGuideSection(button.dataset.guideSection));
    });
    ui.editorFallbackButton.addEventListener("click", () => {
      if (state.editorFallback) state.editorFallback();
    });
    ui.startButton.addEventListener("click", startTask);
    ui.saveButton.addEventListener("click", async () => {
      if (state.busy) return;
      setBusy(true, "正在保存代码…", ui.saveButton, "保存中…");
      try {
        await saveAllDirtyFiles();
      } finally {
        setBusy(false);
      }
    });
    ui.buildButton.addEventListener("click", () => runWorkspaceAction("build"));
    ui.runButton.addEventListener("click", () => runWorkspaceAction("run"));
    ui.appBuildButton.addEventListener("click", runApplicationBuild);
    ui.appExecuteButton.addEventListener("click", runApplication);
    ui.appAction.addEventListener("change", renderApplicationFields);
    ui.serviceStartButton.addEventListener("click", () => startNetworkService(ui.serviceStartButton));
    ui.serviceRefreshButton.addEventListener("click", () => refreshServiceStatus(false, ui.serviceRefreshButton));
    ui.serviceStopButton.addEventListener("click", () => stopNetworkService(false, false, ui.serviceStopButton));
    ui.consoleToggle.addEventListener("click", () => {
      setConsoleMode(ui.consoleDock.dataset.mode === "collapsed" ? "normal" : "collapsed");
    });
    ui.consoleSize.addEventListener("click", () => {
      setConsoleMode(ui.consoleDock.dataset.mode === "large" ? "normal" : "large");
    });
    ui.checkButton.addEventListener("click", () => runWorkspaceAction("check"));
    ui.workflowAction.addEventListener("click", followWorkflowAction);
    ui.hintClose.addEventListener("click", () => closeHint());
    document.querySelectorAll("[data-lesson-section]").forEach((button) => {
      button.addEventListener("click", () => jumpToLessonSection(button.dataset.lessonSection));
    });
    ui.historyButton.addEventListener("click", async () => {
      if (state.task) {
        await loadTaskHistory(state.task.id);
        ui.historyClose.focus();
      }
    });
    ui.historyClose.addEventListener("click", () => { ui.historyPanel.hidden = true; });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && !ui.guideDialog.open && !ui.historyPanel.hidden) {
        event.preventDefault();
        ui.historyPanel.hidden = true;
        ui.historyButton.focus();
      }
    });
    ui.continueButton.addEventListener("click", () => {
      if (!state.progressFresh || state.busy) return;
      const next = state.tasks.find((task) => ["IN PROGRESS", "NEEDS WORK", "STALE"].includes(task.state))
        || state.tasks.find((task) => task.state === "AVAILABLE");
      if (next) selectTask(next.id);
    });
    ui.resumeButton.addEventListener("click", () => {
      const taskId = learningPosition().lastTaskId;
      if (taskId && state.progressFresh) selectTask(taskId);
    });
    window.addEventListener("hashchange", followLearningLocation);
    ui.editor.addEventListener("input", handleEditorInput);
    ui.editor.addEventListener("keydown", (event) => {
      if (event.key === "Tab" && !ui.editor.readOnly && !ui.editor.disabled) {
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
      const native = health.execution_environment === "native-windows";
      const environment = $("execution-environment");
      if (environment) environment.textContent = native ? "本地运行 · Windows 原生" : "本地运行 · Linux / WSL2";
      ui.engine.classList.add("online");
      ui.engine.innerHTML = '<span class="pulse"></span>本地引擎已连接';
      if (await loadTasks()) await followLearningLocation();
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
