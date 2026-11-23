# Bilingual Lessons Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for integration and
> superpowers:dispatching-parallel-agents for independent translation batches.

**Goal:** Let students read every registered lesson and progressive hint in Chinese
or English without disrupting their learning workspace.

**Architecture:** Versioned Markdown siblings travel with the course package.
Existing API payloads gain translation maps; the UI swaps local document content
using a remembered language preference. Existing English API fields stay intact.

**Tech Stack:** Python standard-library HTTP server, vanilla JavaScript/CSS,
Markdown, unittest, Node test runner and Playwright/Chromium.

**Spec:** `docs/superpowers/specs/2026-11-23-bilingual-lessons-design.md`

## Global Constraints

- All 23 registered lessons and all 69 hint levels are covered.
- Preserve English originals and exact code/command/protocol text.
- Preserve the current style, terminal, learner code and formal acceptance.
- Windows-native migration remains paused.

## Review Focus

- A hint response arriving after a language switch must use the latest language.
- Language changes during unsaved editing or running operations must not write or
  discard source, restart workspaces, change task state, or clear the terminal.
- Missing translations and blocked browser storage must leave readable content.
- Translated section shortcuts, file links and bilingual search must still work.
- Narrow screens must retain the previously fixed terminal and button usability.

## Task 1: Bilingual documents and read-only API

**Files:** `course/lessons/**/*.zh-CN.md`, `web/server/cppapplab_web.py`,
`web/server/tests/test_web_workbench.py`.

**Interface:** Task/catalog `title_translations`, task `lesson_translations`, hint
`translations`, each keyed by `en` and optionally `zh-CN`. Preserve existing
`title`, `lesson_markdown` and hint `markdown` English fields.

- [x] Add server tests for paired documents, missing translation, Chinese hint
  splitting and existing bounded/symlink-safe file handling. Run them red.
- [x] Translate three independent content batches and implement bounded sibling
  loading. Derive translated titles from the first H1.
- [x] Run server unittest suite, and validate all catalog translations, section
  structure and exact fenced-code parity.

## Task 2: In-place language switching

**Files:** `web/ui/app.js`, `web/ui/index.html`, `web/ui/app.css`,
`tests/test_web_guidance_state.cjs`.

**Interface:** `changeLessonLanguage(language)` accepts only `zh-CN` or `en`;
`cppapplab.lesson-language.v1` stores the successful local selection.

- [x] Add failing state tests covering default/persistence, dirty-code and console
  preservation, delayed hints, and missing-translation fallback.
- [x] Implement the accessible selector, translated titles/search, body/hints and
  language-independent section shortcuts without reselecting the task.
- [x] Run the existing 71 state/renderer tests and new localization tests.

## Task 3: Browser evidence and integration

**Files:** `tests/test_web_lesson_language.cjs`, existing browser tests/CI wiring,
`README.md` (brief usage note).

- [ ] Capture the untouched main calculator page as the baseline.
- [ ] Exercise real-engine language switching, reload, hints, safe file links,
  unchanged source/progress and 1440/390/320px layouts. Save actual screenshots.
- [ ] Keep existing English browser scenarios explicit about their language and
  add the bilingual journey to CI.
- [ ] Run meaningful regression checks, obtain independent code review, resolve
  material findings, commit/push and follow the established PR/CI/merge workflow.

## Execution record

- Baseline: Python server 39/39; Node state/renderer 71/71.
- Added API/state tests failed before the language implementation, then passed.
- Current focused suites: Python 46/46; Node 77/77.
- Independent translation audit: 23 lessons and 69 hint levels; all 54 fenced
  examples and 645 inline-code occurrences preserved exactly.
- Independent review found and verified fixes for partial-hint fallback, catalog
  resilience to a damaged document, and pending-hint status accuracy.
- Implementation decision: preload both bundled translations so language changes
  cannot trigger writes, workspace resets or a stale language-response race.
- Browser execution is delegated to the existing CI environment because the local
  Chromium process-singleton socket is unavailable. Screenshots remain pending.
