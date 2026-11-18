# Learning Session Recovery Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for root integration and focused independent agents for the two isolated editor/reader tasks. Obtain one fresh whole-branch review before merge.

**Goal:** Restore course/file context after interruption and keep the existing learning UI usable through editor loading and instructional edge cases.

**Architecture:** Extend browser presentation state only, retain the real workspace/acceptance APIs, and reuse the existing fallback editor and escaped renderer. Independent agents work in separate Git worktrees; root integrates their bounded commits with session navigation.

**Tech Stack:** Existing JavaScript/CSS/HTML, dependency-free Node state tests, Playwright and the real C++/Python learning engine.

**Spec:** `docs/superpowers/specs/2026-11-18-learning-session-recovery-design.md`.

## Global constraints

- Preserve all 23 catalog rows, learner sources, prerequisites and fixed acceptance; add no runtime dependency or server mutation endpoint.
- Persist only task/file identifiers. The server remains authoritative for source content and progress.
- Monaco initialization has one total 8000 ms deadline and one final outcome; late callbacks cannot replace fallback edits.
- Keep user-facing instructions in Chinese and avoid claiming observed comprehension from structural/browser checks.
- Local worktree histories are synthetic artifact snapshots; remote Git-data commits must descend from actual main `ea16613c491330e8be5df9179287ffe5a85ed7e9`, tree `e29d037a370cc00cda166711ff89804128dc3506`.

## Review focus

- Busy navigation and failed saves must preserve source and the last successful URL, including browser Back/Forward.
- Malformed links, corrupt or denied storage and removed filenames must degrade safely without arbitrary reads.
- Destination metadata and workspace-list failures must retain coherent lesson/editor/retry state.
- Late loader and AMD callbacks after manual/timeout fallback must have no source/editor side effects.
- Table separators, code fences, escaped pipes, HTML-like input and mobile internal scrolling must remain safe and readable.

## Task 1: Session position and safe navigation (root)

**Files:** `web/ui/app.js`, `web/ui/index.html`, `web/ui/app.css`, `tests/test_web_guidance_state.cjs`, focused browser test, docs.

**Interfaces:** `selectTask(taskId, options)` accepts a preferred file and history mode; `loadWorkspaceFiles(preferredFile)` and `loadFile(path)` report success. Hash targets must resolve through `state.tasks` and the current files response. A separate welcome resume action uses validated recent metadata; `retryWorkspaceFiles()` retries GET without starting a task.

- [x] Reproduce explicit route ignored, destination-fetch context loss and workspace-error lesson replacement with real-function Node tests.
- [x] Implement bounded recent-position parsing, URL updates/restoration, named welcome return, remembered per-task files and guarded history navigation.
- [x] Preserve context until destination metadata succeeds; separate workspace errors and add retry guidance without replacing the lesson.
- [x] Run old and new state regressions; extend real browser coverage for non-first file save/reload, course Back/Forward and invalid route/file recovery.

## Task 2: Bounded editor startup (isolated agent)

**Files:** `web/ui/app.js` editor-loading functions only; loader-action HTML/CSS; new `tests/test_web_editor_loading.cjs`.

**Interfaces:** Preserve `ensureEditorReady(): Promise<boolean>`, resolving true for Monaco or false for the existing textarea. Add an immediate basic-editor action only during loading; retain existing model/save/read-only behavior.

- [x] Add deterministic tests for stalled loader, manual fallback, delayed loader/AMD completion and normal success/immediate failure.
- [x] Observe expected failures, implement one 8000 ms deadline/settlement guard, and verify the focused tests plus existing state suite.
- [x] Commit only owned changes locally; report the commit for root integration. Do not push synthetic history.

## Task 3: Readable tables and accurate application feedback (isolated agent)

**Files:** `web/ui/app.js` Markdown renderer and application result copy only; table styles in `web/ui/app.css`; new `tests/test_web_markdown_tables.cjs`.

**Interfaces:** Preserve `renderMarkdown(source): string`, reuse escaping/inline formatting, and expose no raw HTML. Keep existing API success semantics; nonzero application output is diagnostic rather than an unconditional instruction to repair code.

- [x] Reproduce the three released lessons' missing tables; cover ordinary pipes, code fences, inline code, HTML escaping and escaped separators.
- [x] Implement semantic tables with a bounded keyboard-focusable wrapper and neutral application result wording including the documented calculator negative case.
- [x] Run the renderer/state tests and commit owned changes locally; root integrates and extends the existing real calculator browser journey.

## Integration and closure

- [ ] Integrate disjoint agent commits, add the focused tests to existing CI and exercise stalled-CDN editing plus table scrolling in Chromium.
- [x] Record development experience 39 onward, current status and Chinese usage instructions in one batch.
- [x] Obtain one independent whole-branch review, reproduce/fix concrete findings and run the necessary regressions.
- [ ] Create one PR from real main, await applicable CI, inspect screenshots, merge with expected head pinned and verify actual main-push CI. Record final SHA/run evidence in PR closure.

## Implementation evidence

Root integration includes the isolated editor and reader commits, development experience 39–41 and PR/push Node gates. The integrated Node suites pass 55/55. The focused browser suite adds session/history recovery, read retries, pending-CDN timeout/manual fallback and released-table checks; the full browser journey adds the calculator negative case. One independent whole-branch review found failed same-course file navigation, inconsistent file access after an initial read failure, rejected traversal rewriting history, and pending bookmark loss. Five targeted Node cases reproduced those defects before correction, then passed; the related real browser cases were extended. Real Chromium execution, screenshot inspection and remote closure are recorded in the final PR evidence after completion.
