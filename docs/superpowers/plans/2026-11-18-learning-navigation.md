# Learning Navigation Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans to implement the scoped tasks; retain one independent whole-branch review before merge.

**Goal:** Close concrete lesson-to-code, help-reading, next-task and recovery gaps in the browser workbench.

**Architecture:** Extend the existing file API with read-only template-header metadata. Reuse current task operations and authoritative progress, add bounded lesson/help navigation, and correct the existing CLI's missing-build feedback. Keep independent backend/engine work separate from the frontend integration.

**Tech Stack:** Python standard library, browser JavaScript/CSS, C++17, existing unittest/GoogleTest/Playwright.

**Spec:** `docs/superpowers/specs/2026-11-18-learning-navigation.md`.

## Global Constraints

- Keep all 23 catalog entries, editable `source_files`, fixed acceptance fixtures and prerequisite rules unchanged.
- Reference reads use existing bounded, symlink-rejecting repository path checks and the 1 MiB source limit. No arbitrary template-file browser is added.
- Use existing JavaScript, Python, C++17 and Playwright infrastructure; add no runtime dependencies.
- Keep user-facing guidance in clear Chinese and preserve current learner source and autosave behavior.
- Structural/browser checks do not claim first-time learner comprehension.

## Review Focus

- A read-only reference must never enter the dirty/save pipeline, including fallback-editor Tab handling.
- Late acceptance/hint responses must not change another course or reopen closed help.
- Saving PASS source must invalidate visible continuation and update dependency/history state without reload.
- Long hints and section jumps must remain reachable at 390px without horizontal overflow.
- Missing build directories must produce recovery guidance while unsafe paths remain rejected.

### Task 1: Read-only reference headers

**Files:** `web/server/cppapplab_web.py`, `web/server/tests/test_web_workbench.py`.

**Interfaces:** Existing files and file-content GET responses add boolean `read_only`; declared editable files retain their order and use `false`, appended template headers use `true`. PUT resolution is unchanged.

- [x] Write failing HTTP tests for header enumeration/read from template, rejected PUT, unlisted file rejection, symlink/size rejection and no progress changes.
- [x] Run the targeted tests and observe the expected missing-reference failure.
- [x] Implement the bounded template-header selection and GET branch using existing path guards.
- [x] Run the complete Python web unit suite; expect all tests to pass.

### Task 2: First-run build recovery

**Files:** `src/learning.cpp`, `tests/test_web_workbench_integration.py`.

**Interfaces:** Preserve run endpoint shape and nonzero missing-build result; provide a build command in the CLI output before first run. Preserve unsafe-path errors.

- [x] Add regression coverage for run-before-build and unsafe build-directory cases, including unchanged acceptance history.
- [x] Reproduce missing actionable feedback with the real CLI.
- [x] Correct only the missing-directory branch in existing build validation.
- [x] Build the real CLI with the available compiler and run the focused real-engine regression; full CMake/GoogleTest remains a CI gate.

### Task 3: Browser guidance and integration

**Files:** `web/ui/index.html`, `web/ui/app.css`, `web/ui/app.js`, `tests/test_web_workbench_browser.cjs`, new `tests/test_web_learning_guidance.cjs`, `.github/workflows/ci.yml`, Chinese guide and development/status documentation.

**Interfaces:** Consume Task 1 `read_only` metadata; use existing build/run/check results and authoritative task states. Add no mutation endpoint.

- [x] Add a focused real-browser regression for long hints/return/section jumps, busy course navigation, first-task continuation and source-stale refresh. Use temporary test workspaces; isolate the reference-header UI fixture from the real prerequisite acceptance journey.
- [x] Attempt the baseline browser run; record the local Chromium socket restriction before any assertion. Reproduce the state failures with the actual app.js in a dependency-free Node DOM/network adapter, including the independent-review findings; final state suite is 17/17 green.
- [x] Implement reference read-only enforcement and exact declared-path lesson links, bounded hint navigation, a concise next-action guide and task-bound async state updates.
- [x] Extend the existing full browser journey to exercise real W2/W3 reference headers with Monaco and retain the real prerequisite path.
- [ ] Run the focused and full Chromium journeys in CI and inspect desktop/mobile screenshots. Local Chromium launch is unavailable because socket creation is prohibited; syntax, state and real-engine API regressions pass locally.
- [x] Record development experience and status and obtain one independent branch review. Reproduce and fix the five Important findings and address the two concrete browser-test findings.
- [ ] Create one PR, await applicable CI, merge, and verify the actual main push. Record exact run/commit and visual evidence in the PR closure.
