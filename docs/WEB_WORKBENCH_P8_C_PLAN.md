# P8-C — Full Course Experience Plan

## Objective

Complete the learner-facing workflow for all 23 released W1–W9 tasks in the browser while preserving the existing CppAppLab learning engine as the authority for course metadata, prerequisites, build fingerprints, fixed acceptance and evidence.

P8-C is not a terminal emulator and must not become a browser-accessible arbitrary shell. Each task receives a platform-owned interaction profile derived from its registered learner sources and lesson contract.

## C1 — Audit of the 23 registered tasks

The audit uses `course/tasks.tsv` as the authoritative catalog and each task's declared `source_files` plus its learner `main.cpp` contract. It identifies 14 tasks with an application entry point and 9 library/test-only tasks.

| Task ID | Runtime classification | Browser interaction contract |
|---|---|---|
| `w1.hello-world` | One-shot stdout | Run the learner program and show stdout/exit status |
| `w1.calculator` | Structured CLI | Operation + two numeric operands; no free-form command line |
| `w2.todo-cli` | Structured CLI with persistent state | Add/list/done/remove forms; storage isolated to this learner workspace |
| `w3.task-manager` | Structured CLI with persistent state | Add/list/done/remove forms; storage isolated to this learner workspace |
| `w4.quality-gate` | Structured CLI with persistent state | Same task-manager command contract, plus its fixed quality/test suite |
| `w5.system-lab` | Restricted process-runner demonstration | Only platform-owned sample invocations; never accept a user-supplied executable or arbitrary argument vector |
| `w6.thread-pool` | One-shot stdout | Run the learner demonstration and show stdout/exit status |
| `w7.tcp-echo` | Short-lived TCP service + fixed client | Start one loopback server, send a message through the registered client profile, show the echo, then stop/clean up |
| `w7.tcp-chat` | Long-running TCP service | Start/stop/status plus browser-managed chat clients and bounded message exchange |
| `w8.http-protocol` | Structured CLI | Parse a request-line form or generate the fixed sample response |
| `w8.http-parser` | Stdin-driven CLI | Submit one bounded HTTP request text to `parse-stdin`; close stdin after submission |
| `w8.static-files` | Structured CLI | Select an HTTP method and request target against a platform-created workspace-local document root |
| `w8.router` | Stdin-driven CLI | Submit one bounded HTTP request to `parse-stdin`, using a platform-created document root |
| `w8.http-server` | Long-running HTTP service | Start/stop/status plus an HTTP request panel proxied only to the task's own loopback listener |
| `w8.http-connection-policy` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w8.http-keep-alive` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w8.http-connection-guards` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w8.http-graceful-shutdown` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w8.http-observability` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w8.http-operational-load` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w9.http-chunked` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w9.request-target` | Library/test-only | Build, run the registered CTest suite and formal acceptance |
| `w9.static-root` | Library/test-only | Build, run the registered CTest suite and formal acceptance |

### Classification totals

- 14 tasks have a learner application entry point.
- 9 tasks are intentionally library/test-only and must not receive a fabricated application runner.
- 2 one-shot demonstration tasks: W1 Hello World and W6 Thread Pool.
- 6 structured argument-driven CLI tasks: W1 Calculator, W2 Todo, W3 TaskManager, W4 Quality Gate, W8 HTTP Protocol and W8 Static Files.
- 2 stdin-driven CLI tasks: W8 HTTP Parser and W8 Router.
- 1 restricted process-runner task: W5 System Lab.
- 3 network-service tasks: W7 TCP Echo, W7 TCP Chat and W8 HTTP Server.

The W7 Chat and W8 HTTP Server learner templates are deliberately incomplete until the learner implements their server. A failed start must display the real build/runtime diagnostic; it must never substitute a reference implementation or report a false success.

## Architecture constraints

1. **One source of truth:** task IDs, declared learner sources, prerequisites and acceptance contracts continue to come from `course/tasks.tsv` and the C++ learning engine.
2. **Fixed runtime profiles:** a platform-owned profile maps a task ID to a known target, source subset, include directories, link requirements and a finite set of UI actions. The browser never submits a command, executable path, compiler option, CMake argument, working directory, environment map or raw argv array.
3. **Declared-source boundary:** every source compiled for an application profile must be present in that task's declared `source_files`. Profile files and generated build scripts are platform-owned.
4. **Do not execute learner-controlled CMake configuration:** application builds use platform-generated fixed build profiles. Existing acceptance fixtures remain separate and authoritative.
5. **No evidence side effects:** interactive execution and service sessions do not create or update formal acceptance evidence. Only `cppapplab check` may do so.
6. **Workspace isolation:** Todo/TaskManager persistence and HTTP document roots live below the selected learner workspace, not in the user's global home directory or arbitrary filesystem paths.
7. **Loopback only:** service profiles bind to 127.0.0.1 on an ephemeral port. Browser proxy routes can reach only the active task's own registered listener.
8. **Bounded execution:** apply process timeouts, output/input limits, a bounded number of sessions, idle-session expiry, process-group termination and deterministic cleanup on service shutdown.
9. **Safe process runner:** W5 exposes only fixed platform-owned demonstration commands. It must not accept a user-provided program name, path or free-form argument list.
10. **Network service lifecycle:** start, readiness, status, logs, stop and cleanup are explicit states. No orphan server may survive task changes, browser service shutdown or an idle timeout.

## Delivery slices

### P8-C1 — Runtime contract audit (this document)

- [x] Audit all 23 registered tasks against task metadata and learner entry points.
- [x] Classify 14 application tasks and 9 library/test-only tasks.
- [x] Define one-shot, structured CLI, stdin CLI, restricted process-runner and network-service interaction contracts.
- [x] Record security and evidence-boundary requirements before implementation.

### P8-C2 — One-shot and CLI interaction

- [x] Add fixed application build profiles for Calculator, Todo, TaskManager, Quality Gate, System Lab, Thread Pool, HTTP Protocol, HTTP Parser, Static Files and Router, using only declared learner sources.
- [x] Add structured action forms for Calculator, Todo, TaskManager, Quality Gate, System Lab, HTTP Protocol and Static Files.
- [x] Add bounded stdin submission for HTTP Parser and Router; the web service sends HTTP payload via subprocess stdin (not argv) and the C++ process runner uses a bounded temporary input file.
- [x] Add a fixed Echo demonstration for System Lab; the browser cannot submit executable names or arbitrary arguments.
- [x] Keep application output, errors, exit status, timeout and source fingerprint visible; application runs never write formal acceptance evidence.
- [x] Verify per-workspace Todo persistence isolation and stale-build rejection in C++ workflow tests.
- [ ] Add Chromium user-journey tests for every structured CLI and stdin interaction class; Calculator and Todo run against the real learning engine, while W8 Parser/Router forms are rendered and locked-state behavior verified in Chromium. Remaining task types have engine-level coverage but do not yet have full browser journeys.

### P8-C3 — Network-service interaction

- [ ] Add fixed service profiles for TCP Echo, TCP Chat and HTTP Server.
- [ ] Add loopback-only ephemeral-port startup and readiness detection.
- [ ] Add explicit service states, bounded logs, stop controls, process-group cleanup and idle expiry.
- [ ] Add fixed TCP client bridge for Echo and browser-managed chat sessions for Chat.
- [ ] Add HTTP request/response proxying restricted to the active HTTP Server session.
- [ ] Verify service startup failures surface learner diagnostics and never mark acceptance evidence.
- [ ] Add lifecycle, concurrency, timeout and cleanup browser tests.

### P8-C4 — Course state and history

- [ ] Expose current and historical acceptance attempts from the existing learning-engine records.
- [ ] Explain STALE evidence using the source fingerprint difference.
- [ ] Visualize prerequisite dependencies and locked-task reasons.
- [ ] Preserve progress and history after task changes and service restarts.

### P8-C5 — Cross-course user journey

- [ ] Run browser journeys across all 23 tasks and all interaction classes.
- [ ] Verify desktop, narrow mobile and keyboard-only navigation.
- [ ] Verify recoverability after compile failure, failed acceptance, interrupted process, service crash and browser refresh.
- [ ] Review screenshots and task completion flow before P8-C sign-off.

## Acceptance criteria

P8-C is complete only when all 23 registered tasks are represented accurately in the browser; all 14 application tasks have the correct safe interaction contract; the 9 library/test-only tasks remain test-driven; service sessions are bounded and fully cleaned up; formal acceptance remains the only evidence-writing action; and the full browser user journey passes on Ubuntu 22.04/24.04 CI without regressing the CLI, package builds or Windows launcher checks.
