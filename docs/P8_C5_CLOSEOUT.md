# P8-C5 Integrated Closeout

## Outcome

P8-A/P8-B and P8-C1–C5 are implemented and merged through PRs #40–#44. P8-C is closed as an implementation milestone. The next product priority is tutorial clarity and learner success while preserving the existing technical acceptance contracts.

| Delivered slice | PR | Main merge |
|---|---|---|
| Local browser foundation and editor loop | [#40](https://github.com/clonetimer/CppAppLab/pull/40) | `d4cadbf` |
| Runtime contract audit and structured applications | [#41](https://github.com/clonetimer/CppAppLab/pull/41) | `edc8479` |
| Managed network-service sessions | [#42](https://github.com/clonetimer/CppAppLab/pull/42) | `aaef088` |
| Acceptance history and task-state explanations | [#43](https://github.com/clonetimer/CppAppLab/pull/43) | `ffd25b4` |
| Integrated course journeys, recovery and task-first lessons | [#44](https://github.com/clonetimer/CppAppLab/pull/44) | `25686fd210098006ffcadf8891c54f043e0b148d` |

## What learners can use

The authoritative `course/tasks.tsv` catalog contains 23 tasks. All 23 are browser-selectable with lessons, declared source editing, prerequisite/state explanations, task-first guidance, registered build/test and formal acceptance.

- **14 application/service tasks:** Hello World's direct runner, ten structured application profiles and three managed TCP Echo/TCP Chat/HTTP Server profiles.
- **9 library/test-focused tasks:** source editing, the registered CMake/GoogleTest workflow and formal acceptance. Their Run / Tests action runs the task's tests; these exercises do not require a separate standalone application.
- **Shared workbench:** Monaco multi-file editing, offline textarea fallback, autosave/dirty state, source-fingerprint checks, diagnostic navigation and read-only acceptance history.
- **Evidence boundary:** build/run and service demonstrations do not grant formal PASS; only the learning engine's formal check writes course acceptance evidence.

## Verification record

| Evidence | Exact revision/event | Result |
|---|---|---|
| Final P8-C5 PR CI [#1848](https://github.com/clonetimer/CppAppLab/actions/runs/44104945290) | PR head `db8c7c7635d4e8299e4e997aa904c07708cad99c`; pull_request event | 12 jobs succeeded; push-only Ubuntu 22.04 compatibility job skipped |
| Merged-main CI [#1849](https://github.com/clonetimer/CppAppLab/actions/runs/44119538209) | Main `25686fd210098006ffcadf8891c54f043e0b148d`; push event | All five scheduled jobs succeeded, including Ubuntu 22.04 source-build compatibility |
| Browser evidence | Chromium on Ubuntu 24.04 | Desktop/mobile PNGs and catalog-derived course CSV uploaded |
| Core/API, sanitizer and quality matrices | Ubuntu 22.04 and 24.04 in PR CI | Scheduled jobs succeeded; repository-wide formatting step remained manual/opt-in and skipped |
| Packages and offline dependencies | Separate Ubuntu 22.04 and 24.04 targets in PR CI | Package builds/relocation smoke and offline no-install verification succeeded |

The merged-main push skips the full GCC/sanitizer/quality matrices and offline bundle jobs by workflow configuration; those passed on the final PR run. PR artifacts use GitHub's synthetic merge checkout `098687b865209f40ac38863419d9b4e50ff3bcdf`, which is distinct from both the PR head and the final squash merge.

The 23-task Chromium sweep checks catalog representation, lesson rendering, prerequisites, state and task-first guidance. Representative deep journeys cover editing, build/run, acceptance separation, compiler-error recovery, history refresh and dirty-source save/reload recovery. API/process regression covers service lifecycle, crash/restart and shutdown cleanup. These are distinct evidence dimensions; catalog coverage does not claim that all 23 exercises were solved in Chromium.

## Remaining boundaries

- P8-D: clangd/GDB integration.
- P8-E: Windows 11 + WSL2 target-machine browser acceptance and delivery qualification.
- Full keyboard/accessibility review and first-time learner comprehension testing.
- Versioned release publication and the full release evidence package described in [P8_FINAL_DELIVERY_PACKAGE.md](P8_FINAL_DELIVERY_PACKAGE.md). CI artifacts are candidates with limited retention, not a published release.

The user authorized merging completed work after required verification and supplying acceptance feedback afterward. Future usability feedback continues against the delivered milestone without reopening completed implementation work solely for another sign-off.
