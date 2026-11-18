# P8-C — Full Course Experience Plan

**Status: implementation closed after PRs #41–#44.** This document records the delivered contracts and validation scope. See [P8-C5 closeout](P8_C5_CLOSEOUT.md) for exact CI evidence; unchecked follow-up items below remain usability work.

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
3. **Declared-source boundary:** every learner-provided source compiled for an application profile must be present in that task's declared `source_files`. Fixed, explicitly enumerated platform-owned support sources may be linked where the task contract requires them (see the HTTP Server build boundary below). Profile files and generated build scripts are platform-owned.
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
- [x] Add a real Python HTTP-to-subprocess test with a 140,000-byte request exceeding Linux's typical per-argument cap, plus C++ and Python rejection tests above 256 KiB.
- [x] Add a fixed Echo demonstration for System Lab; the browser cannot submit executable names or arbitrary arguments.
- [x] Keep application output, errors, exit status, timeout and source fingerprint visible; application runs never write formal acceptance evidence.
- [x] Verify per-workspace Todo persistence isolation and stale-build rejection in C++ workflow tests.
- [x] Add Chromium journeys for Calculator, Todo and TaskManager using the real learning engine; exercise Todo/TaskManager add-list-done-remove operations, workspace persistence, incomplete learner diagnostics, and separation between application runs and formal acceptance. Verify Quality Gate's real TODO failure and render remaining one-shot/structured profile forms with prerequisite-aware controls. W8 Parser/Router forms are verified in Chromium while bounded stdin transport is covered by real Python subprocess and C++ process tests.

### P8-C3 — Network-service interaction

#### Service-session architecture

- Service profiles are platform-owned and task-ID keyed. The browser supplies only typed request/message data; it never chooses a binary, port, bind address, process arguments or environment.
- Build each service through the registered application build profile before launch. The service supervisor launches only the workspace's fingerprint-stamped `learner_app` with fixed arguments.
- Bind services to `127.0.0.1` only and allocate a short-lived loopback port. The browser reaches services through the same-origin Python API bridge, not by supplying an arbitrary host or URL.
- Use an explicit lifecycle: `STARTING → RUNNING → STOPPING → STOPPED` or `FAILED`. Capture bounded stdout/stderr logs, drain output continuously, reap every child, and terminate the entire process group on stop or timeout.
- Enforce a small active-session limit, a maximum session lifetime, idle expiry, bounded client counts, bounded request/message sizes, and deterministic cleanup on task switch, source mutation and web-server shutdown.
- A service being ready or responding is not formal course acceptance. Only `cppapplab check` may write course evidence.

#### Fixed task contracts

| Task | Fixed service contract | Browser bridge |
|---|---|---|
| `w7.tcp-echo` | One loopback Echo server process; one client exchange per server instance | Run the registered learner client action against the active session port; show exact echoed output |
| `w7.tcp-chat` | One loopback Chat server; bounded multi-client sessions | Python-managed TCP clients using the registered `NICK`, `MSG` and `QUIT` line protocol; no browser raw socket or arbitrary destination |
| `w8.http-server` | One loopback HTTP server using a workspace-local document root | Same-origin request proxy with fixed loopback destination, method allowlist, bounded target/body/response and filtered response headers |

#### Build boundaries

- TCP Echo application profile compiles only its declared learner `main.cpp` and `tcp_echo.cpp`.
- TCP Chat application profile compiles only its declared learner `main.cpp` and `tcp_chat.cpp`.
- HTTP Server compiles learner-declared `main.cpp` and `http_server.cpp`, plus explicitly enumerated platform-owned support sources from the previously validated W8 Router and W6 Thread Pool reference libraries. These support sources are not exposed as learner-editable files and are not accepted from the browser.
- The HTTP Server learner implementation is intentionally incomplete at this milestone. Startup must surface its actual constructor/run TODO; it must not fall back to the reference server.

#### Delivery slices

- [x] C3-A: fixed service build profiles, process supervisor, bounded log capture, state machine, ephemeral port allocation, stop/expiry and process-group cleanup.
- [x] C3-B: TCP Echo start/status/send/stop journey using the learner's own server and client implementation; a C++ workflow test performs a real loopback client/server round trip.
- [x] C3-C: TCP Chat server lifecycle and bounded browser-managed multi-client bridge; API tests cover two clients, join/leave events, message broadcast and input limits.
- [x] C3-D: HTTP Server lifecycle and same-origin HTTP request/response proxy against the workspace document root; API tests cover GET/POST, target validation, request/response size limits and timeout handling.
- [x] C3-E: Chromium profile-panel/prerequisite checks on Ubuntu 24.04, with Python API and C++ process/service regression on Ubuntu 22.04/24.04. Incomplete learner services surface real diagnostics; application/service interactions never create formal PASS evidence. Live service lifecycle and transport behavior are exercised by API/process tests; the browser run does not deeply complete every service lesson.

#### Acceptance gates

1. Service templates default to loopback binding; the browser bridge always connects to 127.0.0.1 and accepts no user-supplied host, port, executable or process argument. The workbench is a local development environment, not an OS sandbox for deliberately modified learner source.
2. Start is idempotent; failed startup returns learner diagnostics; status and logs remain available after process exit.
3. Stop, task switching, source edits, idle expiry and web-server shutdown leave no child process or client socket behind.
4. Logs, clients, chat lines, HTTP request bodies and HTTP responses have explicit limits.
5. Echo completes a real learner-client round trip; Chat supports at least two independent browser-managed clients and message broadcast; HTTP proxy returns a real learner-server response when implemented.
6. Starter Chat/HTTP Server TODOs are shown as failures, never replaced by reference implementations or reported as acceptance PASS.
### P8-C4 — Course state and history

- [x] Expose current and historical acceptance attempts through a read-only per-task API backed by the learning engine's existing history records.
- [x] Explain STALE, NEEDS WORK and LOCKED states in task detail, including source-fingerprint/workspace invalidation and missing prerequisite reasons.
- [x] Show prerequisite task IDs alongside the selected lesson and add an in-workbench acceptance history panel with timestamp, result, test count, workspace and fingerprint.
- [x] Preserve progress and history across task changes and service restarts; application runs and service sessions remain excluded from formal acceptance history.

### P8-C5 — Cross-course user journey (implementation closed)

- [x] Traverse all 23 registered task IDs in Chromium and verify lesson rendering, task state, prerequisites and visible `Start here` guidance. Representative deep journeys exercise editing, building, running and formal acceptance; catalog coverage is not a claim that every exercise was solved in this browser run.
- [x] Verify desktop layout, 390px mobile layout without page-level horizontal overflow, keyboard course selection, and history-panel Enter/focus-transfer/Escape/focus-restoration behavior.
- [x] Verify compile-error correction, failed formal acceptance, retained PASS/history after refresh, and dirty-source cancel-reload/save/reload recovery in Chromium.
- [x] Verify service crash/restart and web-server shutdown process cleanup through API/process regression.
- [x] Generate desktop/mobile PNGs and the catalog-derived course matrix in passing CI [#1848](https://github.com/clonetimer/CppAppLab/actions/runs/44104945290).
- [ ] Follow-up: complete the course/editor keyboard tab-order and accessibility audit.
- [ ] Follow-up: review first-time learner task completion and comprehension. Screenshot generation and structural checks do not certify these outcomes.

## Accepted validation scope

All 23 registered tasks are represented in the browser; 14 application/service tasks have fixed runtime interactions; the 9 library/test-focused tasks use registered build/test/acceptance; service sessions have bounded lifecycles and cleanup; and formal acceptance remains the only course-evidence-writing action. Chromium journeys pass on Ubuntu 24.04, while API and C++ regression pass on Ubuntu 22.04/24.04. CLI regressions, both package targets and Windows launcher checks pass. The merged-main push run [#1849](https://github.com/clonetimer/CppAppLab/actions/runs/44119538209) also passed Ubuntu 22.04 source-build compatibility.

This records the implemented CI matrix. Windows 11 + WSL2 target-machine browser acceptance, full accessibility and learner comprehension remain separately identified work. The release-package schema is in `docs/P8_FINAL_DELIVERY_PACKAGE.md`; a versioned release has not been published as part of this implementation closeout.

### Curriculum-wide task-first teaching

- [x] W1–W4 first-task walkthroughs with staged complexity and plain-language mental models.
- [x] W5–W9 all 18 advanced task lessons now begin with a task-specific first path and a concrete mental model.
- [x] Chromium catalog traversal now asserts that all 23 lessons render a visible `Start here` section.
- [ ] Review actual beginner usability with a first-time learner walkthrough; automated presence checks do not replace comprehension testing.

### Tutorial accessibility

- [x] Add a task-first walkthrough and plain-language terminology to the Chinese learner guide.
- [x] Add step-by-step "Start here" sections to W1–W4 lessons, preserving the existing technical contracts and acceptance criteria.
- [x] Extend the same teaching structure to W5–W9 and verify all 23 lesson pages in Chromium.
- [ ] Review task-level help, hints and error explanations for consistency with the beginner-first path.
