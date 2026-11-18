# Chinese learning guide implementation plan

**Spec:** `docs/superpowers/specs/2026-11-18-chinese-learning-guide-design.md`

**Baseline:** remote main `09079bf896f54d11280f0523723577fed38391bb`, tree `bcf5379ac6577ed6459c71c4231c69fb91a61068`; verified local artifact snapshot `d6758bd555025e5307f0c1124cdb72dd611c2942`.

## Global constraints

- Reuse the existing isolated root worktree. Local commits have synthetic artifact ancestry and must never be pushed; publish changed blobs using GitHub Git-data against actual remote main.
- Follow the user's standing authorization to finish, verify and merge without further approval pauses.
- Keep this batch on web learner orientation and course entry. Do not expand curriculum or acceptance requirements, add dependencies, reopen completed recovery work, or retry local Chromium in the restricted environment.
- Implement the API and UI inline after the independent reconnaissance; delegate the independent browser test additions in a separate existing worktree, then use one fresh whole-branch reviewer. Keep task evidence in a separate scratch ledger because the cloud skill's optional helper scripts are unavailable through the resource provider.

## Task 1: Read-only authoritative guide endpoint

**Files:** `web/server/cppapplab_web.py`, `web/server/tests/test_web_workbench.py`.

**Interface:** `GET /api/v1/learning-guide` returns only `{markdown}` from the fixed guide path with the existing 64 KiB lesson limit. It never calls the CLI or creates a workspace.

- [x] Add focused API cases and observe the new route fail before implementation.
- [x] Implement the bounded fixed-path read; cover missing, oversized, invalid UTF-8, symlink/non-regular files and extra path segments.
- [x] Run `python3 -m unittest discover -s web/server/tests -p 'test_*.py'`; expected: all cases pass. Result: 38/38 pass locally.

## Task 2: Guide dialog and guarded course navigation

**Files:** `web/ui/index.html`, `web/ui/app.js`, `web/ui/app.css`, `tests/test_web_guidance_state.cjs`.

**Consumes:** Task 1's fixed endpoint; current `state.tasks`, escaped `renderMarkdown`, and `selectTask(taskId)` result.

- [x] Add actual-function Node cases; observe absent guide behavior fail.
- [x] Implement native-dialog open/close/focus, one shared GET request/cache, error/retry, section shortcuts and exact catalog-ID enhancement. Keep presentation independent of editor/task state.
- [x] Guard guide task switching through existing navigation and keep readable feedback in the dialog when a switch is refused.
- [x] Correct the welcome example's newline.
- [x] Run `node --test tests/test_web_guidance_state.cjs tests/test_web_editor_loading.cjs tests/test_web_markdown_tables.cjs`; expected: all cases pass. Result: 67/67 pass locally.

## Task 3: Real-browser coverage, documentation and closure

**Files:** `tests/test_web_learning_guidance.cjs`, `course/LEARNING_GUIDE_ZH.md`, `docs/DEVELOPMENT_EXPERIENCE.md`, `docs/STATUS.md`, this plan.

- [x] Extend the existing focused Chromium journey for first-use/course guide access, all 23 catalog entries, desktop/390 px navigation and focus, retry, locked-course reading and protected dirty work. Keep existing CI jobs and frameworks.
- [x] Clarify browser/CLI guide instructions and record development experience 42 onward plus PR49's verified closure.
- [x] Run course material and Node regressions plus syntax/whitespace checks; expected: pass. Browser execution remains in CI.
- [x] Obtain one independent whole-branch review. Reproduce and fix concrete important findings with targeted RED→GREEN tests, then run the affected suite.
- [ ] Publish one PR from actual main; await its final-head applicable CI, inspect guide screenshots, merge with expected head pinned, and verify actual main-push CI. Put final SHA/run evidence into the PR closure.

## Review focus

- A guide loaded before catalog readiness must acquire exact known course controls when the catalog arrives; failed health must not prevent reading.
- Delayed guide reads and task selection must not reopen a closed dialog, steal focus or discard code. Native Escape should close the guide without also closing an underlying history panel.
- Only guide task headings become controls; unknown/planned IDs, inline code, HTML and relative Markdown syntax must not acquire arbitrary navigation authority.
- Locked lessons remain readable; busy or save failure retains the original task/file and keeps the guide usable. Successful navigation returns focus to a useful course element rather than a now-hidden welcome control.
- Header/close controls and section shortcuts remain usable at 390 px, with tables scrolling locally and no whole-page overflow.

## Review outcome

One independent whole-branch review found late workspace-read autofocus, the first-use entry retaining an unrelated reading position, and focus restoration to a welcome entry hidden during initial bookmark navigation. Four actual-app tests reproduced these behaviors before the targeted fixes; the combined Node suites now pass 71/71. The real-browser cases include the corresponding user paths. A hypothetical future guide heading with both strong and inline-code formatting remains a documented minor; current course entries and catalog-bound navigation are unaffected. Final CI, screenshots and merge evidence are recorded in the PR closure.
