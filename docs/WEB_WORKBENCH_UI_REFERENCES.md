# P8 UI design references and learner-experience rules

## Reference products

These are behavioral and information-architecture references, not a license to reproduce another product's visual assets or copy its screen wholesale.

| Mature reference | What CppAppLab adopts | What CppAppLab deliberately avoids |
|---|---|---|
| [Visual Studio Code — User Interface](https://code.visualstudio.com/docs/getstarted/userinterface) | Editor-first desktop workspace, source tabs, distinct diagnostics/output area, navigation that does not compete with code | Exposing every advanced IDE menu to a beginning learner |
| [GitHub Codespaces](https://docs.github.com/en/codespaces/developing-in-a-codespace) | A single browser tab for the code/workspace/build/run loop, explicit local-service readiness and recovery | Cloud account or remote container requirements for local learning |
| [Exercism](https://exercism.org/docs/building/tracks) | Curriculum progress, prerequisites, contextual instructions and progressive hints tied to the task | Turning an exercise into a passive documentation page without real execution |

## Current design decision (P8-C2)

**Desktop:** persistent compact course navigation on the left; code editor takes the larger central column; current-task lesson occupies the narrower right column; build/run/acceptance diagnostics stay next to the code. The active application form is fixed to a registered task profile and appears only when that task has a learner entry point.

**Narrow/mobile:** a horizontally scrollable compact course selector appears first; the lesson remains before the editor so beginners can read before writing; the code editor retains its own horizontal scrolling, not page-wide overflow. Application fields wrap and preserve proper labels.

## Information priority

1. Current task, prerequisite status and clear start/resume action.
2. Source code and source-file tabs.
3. Build, program run and formal acceptance as **separate** operations.
4. Short actionable compiler diagnostics and actual learner program output.
5. Contextual lesson and three progressive hints.
6. Advanced tooling belongs to P8-D; it is not injected into the P8-C beginner interface.

## Typography and controls

- Lesson content: 14px body with generous line spacing; headings remain distinct.
- Monaco code: 14px default; fallback editor must remain usable without CDN access.
- Application labels/help: at least 11px; controls use a 36px minimum touch target in the compact layout.
- Display human-readable operation names (e.g. 加法（add）) while retaining the exact fixed CLI values.
- Mobile layouts must have no page-level horizontal overflow at 390px; the editor may scroll horizontally within its own surface.

## State and error semantics

- An available task is not the same as an active workspace; explain the next step.
- A locked task may show the interaction form for educational preview, but Build/Run remain disabled and the reason is written in learner-facing language.
- Build means compilation, Run means executing the learner's code, Formal Acceptance means writing the course evidence. Never show a successful application run as a formal PASS.
- Incomplete learner code must show its real compile error, TODO or nonzero exit code. Never switch to reference code silently.
- Input forms are constrained by registered task profiles. No general command shell, executable path, user-defined argv list or unbounded HTTP request fields.

## Verification

The Playwright/Chromium browser suite checks the editor-left/lesson-right desktop geometry, lesson-before-editor mobile layout, 390px overflow, actual W1 Calculator and W2 Todo interactions, W8 locked HTTP form rendering, Monaco editor behavior, source autosave and CDN-blocked editor fallback. Screenshots are retained as artifacts of the [CppAppLab GitHub Actions CI](https://github.com/clonetimer/CppAppLab/actions/workflows/ci.yml).

Review future screenshots at normal 100% browser zoom, checking typographic readability, relevant visible controls and error recovery in addition to numerical tests. UI similarity to reference products is **not** an acceptance criterion; a learner completing a task without leaving the browser is.
