# Learning Space Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Make lesson prose, source and current operation results usable in the existing workspace.

**Architecture:** Keep the current DOM/API and add an independent terminal presentation state. Compact existing headers and derive the Start/workflow presentation from real workspace/action state. Parallel workers own tests while the main implementer owns production UI.

**Tech Stack:** Existing browser JavaScript/CSS/HTML, Node test runner, Python server, Playwright/Chromium and real C++ learning engine.

**Spec:** `docs/superpowers/specs/2026-11-23-learning-space-design.md`

## Global Constraints

- Preserve existing visual style and Chinese/English course switching.
- No Windows-native migration, curriculum changes or acceptance-rule changes.
- Never fabricate output/PASS for screenshots; baseline is exact PR base.
- Browser execution uses CI because this executor cannot launch Chromium safely.

## Review Focus

- Manual terminal collapse during slow requests must retain the new result summary.
- A failed save must cancel execution and restore the actual clicked action.
- Restored/stale/locked workspaces must expose correct actions without resetting source.
- Runtime/API errors must not overwrite formal acceptance history or PASS.
- Long translations and small viewports must retain real readable content and focus.

### Task 1: Terminal presentation and visible action lifecycle

**Files:** `web/ui/app.js`, `web/ui/index.html`, `tests/test_web_guidance_state.cjs`

**Interfaces:** `setConsoleMode(mode)`, `beginConsoleOperation(title, progress)`,
`finishConsoleOperation(passed, summary?)`, `resetConsole()`; dock dataset
`state=idle|running|success|error`, `mode=collapsed|normal|large`.

- [x] Add targeted failing tests for lifecycle, manual collapse, task reset,
  workflow pending visibility, failed-save cancellation and workspace-ready status.
- [x] Implement the state helpers and attach all existing output-producing paths.
  Keep service logs independent. Pass the workflow trigger to real action handlers.
- [x] Run `node --test tests/test_web_guidance_state.cjs tests/test_web_editor_loading.cjs tests/test_web_markdown_tables.cjs` and inspect all results.

### Task 2: Reclaim reading/editing space

**Files:** `web/ui/app.css`, `web/ui/index.html`, `web/ui/app.js`

**Interfaces:** existing panel/heading/lesson IDs retained; `#task-details`,
`#console-summary`, `#console-size`; `#workspace-state` is a neutral task-header span.

- [x] Compact headers, move metadata to details, scope duplicate lesson H1 hiding.
- [x] Replace fixed dock reservation with actual topbar/remaining viewport layout
  and the exact sizes in the spec. Retain intentional mobile scrolling.
- [x] Check syntax/diff and existing state/Markdown behavior.

### Task 3: Browser evidence, review and delivery

**Files:** `tests/test_web_learning_space.cjs`, existing browser tests,
`.github/workflows/ci.yml`, development experience notes.

- [ ] Capture unchanged baseline and changed screenshots/geometry for each of
  the three issues; assert actual clipped visible prose/source dimensions.
- [ ] Exercise real build success/error, pending, collapse, enlarge and recovery.
  Update old oversized-dock assertions to phase-specific usability requirements.
- [x] Review the production diff independently and resolve material findings.
- [ ] Run CI and inspect actual screenshots. Record decisions and observed limits.
- [ ] Merge after required checks pass, verify main CI, and deliver three-item
  before/after evidence with update instructions.
