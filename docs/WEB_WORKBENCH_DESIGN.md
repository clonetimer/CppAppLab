# P8 — Web Learning Workbench

## Goal

Add a browser-first learning experience without replacing the existing C++ learning engine. Learners should be able to browse the course, start or resume a task, read its lesson, edit declared source files, and run the platform-owned acceptance check from one browser tab.

The CLI remains supported for advanced users, automation, CI, and recovery.

## Architectural boundary

```text
Windows browser
  |
  | same-origin HTTP on loopback
  v
CppAppLab local web service (Python standard library)
  |-- static learning UI
  |-- course/task read API
  |-- workspace file API (task-declared files only)
  |-- fixed start / hint / build / run / check actions
  |
  v
Existing CppAppLab CLI and learning engine
  |-- course/tasks.tsv
  |-- lesson and hint Markdown
  |-- learner workspace lifecycle
  |-- GCC / CMake / GoogleTest / CTest acceptance
  |-- progress and evidence records
  v
Ubuntu 22.04 / 24.04 under WSL2
```

The web service is an adapter, not a second learning engine. It must delegate workspace creation, prerequisite enforcement, source fingerprinting, acceptance, and progress updates to the existing CppAppLab executable.

## P8-A scope

1. Add a dependency-free local HTTP service using Python's standard library.
2. Bind to loopback only; reject unexpected Host and Origin headers.
3. Issue an in-memory CSRF token and require it for every mutating API request.
4. Serve the browser UI and same-origin JSON API from one origin.
5. Read task metadata from the authoritative `course/tasks.tsv` catalog.
6. Start or resume workspaces through `cppapplab learn <task-id>`.
7. Expose only source files declared by the selected task.
8. Save source files atomically and reject path traversal, symlinks, undeclared files, and oversized payloads.
9. Run only fixed platform actions; never expose an arbitrary shell/command endpoint.
10. Run the existing fixed acceptance check through `cppapplab check <workspace>`.
11. Add API, path-safety, and browser smoke tests.
12. Keep the existing CLI and current Windows launcher behavior intact until browser startup is separately qualified.

## Initial API contract

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/v1/session` | Obtain the per-process CSRF token |
| GET | `/api/v1/health` | Check service and CppAppLab engine |
| GET | `/api/v1/tasks` | List catalog tasks and current progress states |
| GET | `/api/v1/tasks/{task_id}` | Read task metadata and lesson |
| GET | `/api/v1/tasks/{task_id}/hints/{1..3}` | Read one progressive hint |
| POST | `/api/v1/tasks/{task_id}/start` | Start or resume using the learning engine |
| GET | `/api/v1/workspaces/{task_id}/files` | List declared learner files |
| GET | `/api/v1/workspaces/{task_id}/files/{path}` | Read a declared learner file |
| PUT | `/api/v1/workspaces/{task_id}/files/{path}` | Atomically save a declared learner file |
| POST | `/api/v1/workspaces/{task_id}/build` | Run the task's fixed compiler/build profile |
| POST | `/api/v1/workspaces/{task_id}/run` | Run the direct learner program or fixed CTest suite |
| POST | `/api/v1/workspaces/{task_id}/check` | Run fixed platform acceptance and record evidence |

No route accepts arbitrary shell commands, compiler flags, build directories, or executable paths from the browser.

## Security and correctness requirements

- Default bind address is `127.0.0.1`; non-loopback binding is rejected.
- Only loopback Host values on the selected port are accepted.
- State-changing requests require same-origin requests, JSON content types, and the session CSRF token.
- The service does not enable CORS.
- Task IDs and file paths are validated against the course catalog.
- Workspace operations are restricted to each task's declared `source_files`.
- Symlinks and non-regular source files are rejected.
- File writes use a temporary sibling file and atomic replacement.
- Request bodies, lesson files, source files, and process output have explicit size limits.
- The server uses argument arrays with `subprocess`; it never invokes a shell.
- Build, run and acceptance are fixed CppAppLab CLI actions; the browser cannot supply commands, flags, executable paths, CMake arguments or test filters.
- Direct-execution tasks use a fixed C++17 compiler profile, `/dev/null` stdin, a 30-second build limit, a 5-second run limit and bounded output. CMake/GoogleTest tasks build only the registered platform fixture and run its fixed CTest suite with a 45-second limit.
- A successful build records a source fingerprint. Run is refused when the current source differs from the built fingerprint. Build/run never create acceptance evidence; only `check` records PASS/FAIL evidence.
- The enhanced Monaco editor is pinned to a versioned CDN URL; if the CDN is unavailable, the UI falls back to the local plain-text editor. The Python service and course API remain dependency-free.
- The service is intended for one local learner session, not LAN or multi-tenant hosting.

## Delivery phases

### P8-A — Local web foundation
- HTTP service, same-origin API, browser course shell, task start/resume, source file read/write, fixed acceptance endpoint, security and CI smoke coverage.

### P8-B — Editor and learning loop
- Monaco 0.52.2 editor with a local plain-text fallback, per-file models and multi-file tabs.
- Debounced autosave, version-aware in-flight save protection, dirty-state indicators and Ctrl/Cmd+S.
- Separate fixed-profile build, run/test and formal acceptance actions.
- Source-fingerprint build stamps prevent running binaries built from older source.
- GCC/CMake diagnostics are parsed into clickable file/line entries and Monaco markers.
- Progressive hints and responsive layout remain available.
- Preserve the fixed acceptance contract: build/run do not write PASS evidence; only formal acceptance does.

### P8-C — Full course experience
- All 23 released W1–W9 tasks, interactive program input/output, network-service tasks, progress/history views, and task prerequisite visualization.

### P8-D — Professional tooling
- clangd diagnostics/completion and GDB debugging workflows, subject to separate security and process-lifecycle review.

### P8-E — Windows/WSL2 delivery
- Double-click browser startup, health/readiness checks, safe service shutdown/restart, packaged static assets, and target-machine acceptance on Ubuntu 22.04 and 24.04.

## Acceptance gates

P8-A is complete only when:
- the service starts without third-party Python packages;
- the browser UI can load the task catalog and lesson;
- W1 Hello World can be started/resumed, edited, saved, and checked from the browser;
- a passing check is recorded by the existing learning engine;
- a source edit makes previous PASS evidence stale through the existing fingerprint contract;
- locked tasks remain locked;
- traversal, symlink, undeclared-file, wrong-Origin, missing-token, oversized-body, and arbitrary-command attempts are rejected;
- service and API smoke tests pass on Ubuntu 22.04 and 24.04;
- the existing CLI, Windows launcher, and course acceptance tests remain green.
