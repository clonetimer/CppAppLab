# CppAppLab Roadmap

## P0 — Foundation

**Status: merged into `main`.**

- Repository layout and engineering conventions
- WSL2 + Ubuntu 24.04 + GCC + CMake + GDB environment contract
- C++17 build target and minimal CLI
- GoogleTest / CTest baseline
- clang-format, clang-tidy, ASan, UBSan configuration
- GitHub Actions build, test, sanitizer, and quality jobs
- Course map and learning/acceptance contract

## P1 — Learning engine

### P1-A — W1 Hello World

**Status: merged into `main`.**

- task registry and CLI: `list/start/lesson/hint/check/status/next`
- learner workspace marker and no-overwrite workspace creation
- reference and learner source separation
- fixed GCC compile + exact-output acceptance
- source fingerprint and stale local evidence handling
- symlink/path-traversal rejection for declared source paths
- process timeout, output cap, stdin isolation, and process-group termination

Hosted Ubuntu CI passed 15/15 tests in both ordinary and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

### P1-B — W1 multi-file Calculator

**Status: merged into `main`.**

- second registered learning task
- separate interface, implementation, and command-line front end
- platform-owned CMake/GoogleTest acceptance fixture
- seven fixed arithmetic/error-path tests
- two CLI smoke checks for normal arithmetic and division by zero
- source staging prevents learner build files and headers from replacing the platform fixture

Hosted Ubuntu CI passed 17/17 tests in both ordinary and ASan/UBSan builds.

### P1-C — persistent progress and workspace recovery

**Status: merged into `main` via PR #4.**

- external task catalog in `course/tasks.tsv`
- validated task metadata and prerequisite ordering
- cross-workspace prerequisite checks using current source fingerprints
- persistent progress outside the source checkout
- workspace registration at `start`, before the first check
- `progress`, `resume <task-id>`, and `history <task-id>` commands
- latest result plus bounded per-task check history
- multiple workspaces per task; a current PASS in any workspace can satisfy a prerequisite
- source edits, failed latest attempts, or deleted workspaces invalidate that workspace's PASS

Hosted Ubuntu 24.04 CI passed 28/28 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

### P1 follow-up

- additional W1–W8 learner/reference task packs
- richer task-specific diagnostics and feedback
- verified offline dependency package
- local WSL2 acceptance and first release preparation

### P2-A — W2 persistent Todo CLI

**Status: merged into `main` via PR #5.**

- STL-backed Todo model with `std::vector`, standard algorithms, and `std::unordered_map`
- add/list/done/remove CLI commands
- versioned text persistence with escaped title fields
- load validates into a temporary collection before replacing current data
- save publishes a complete temporary file by rename
- platform-owned CMake/GoogleTest fixture with 18 unit tests and one multi-process CLI workflow test
- task depends on a current W1 Calculator PASS
- hosted Ubuntu CI passed 30/30 root tests in GCC and ASan/UBSan; clang-format and clang-tidy-enabled checks passed

**P1 exit criteria:** a learner can start a task, read it, request hints, implement it, run fixed platform-owned checks, inspect failures, and resume without losing work. A local evidence record is navigation data, not a certificate.

## P2 — W1 to W3: C++ foundations

- W1: toolchain, syntax, functions, multi-file compilation, CMake, GDB, calculator
- W2: STL, algorithms, lambdas, file persistence, Todo CLI (P2-A)
- W3: classes, RAII, smart pointers, copy/move, operators, TaskManager refactor (P2-B merged into `main` via PR #6)

### P2-B — W3 Task / TaskManager Refactor

**Status: merged into `main` via PR #6.**

- `Task` value object with validated invariants and ID-based identity/order
- `TaskManager` uses `std::vector<std::unique_ptr<Task>>` for exclusive ownership
- deep-copy semantics, copy-and-swap assignment, and explicit move semantics
- non-owning lookup and ordered value snapshots
- W2 file-format compatibility and transactional load
- platform-owned fixture: 30 unit tests plus one multi-process CLI workflow test
- hosted Ubuntu CI passed 32/32 root tests in GCC and ASan/UBSan; clang-format and clang-tidy-enabled checks passed

### P3-A — W4 TaskManager Quality Gate

**Status: merged into `main` via PR #7; hosted Ubuntu CI passed.**

- ten learner-authored GoogleTest cases with meaningful TODO prompts
- CMake Debug build, CTest, ASan/UBSan, clang-format and clang-tidy configurations
- GDB debugging workflow and optional Valgrind diagnostics
- platform fixture: 30 fixed behavior tests + 10 learner tests + CLI workflow (41 total)
- hosted Ubuntu CI passed 34/34 root tests in GCC and ASan/UBSan; clang-format and clang-tidy-enabled checks passed
- W4 depends on a current PASS for W3 TaskManager

## P3 — W4: engineering quality

- GoogleTest design and boundary/error-path coverage
- GDB with tests
- ASan / UBSan and Valgrind
- compiler warnings, clang-format, clang-tidy
- GitHub Actions, versioning, release notes

### P3-B — W5 Linux System Programming Lab

**Status: merged into `main` via PR #8; hosted Ubuntu 24.04 CI passed.**

- bounded thread-safe queue and producer/consumer coordination
- POSIX pipe, fork, dup2, execvp, poll, read/write and waitpid
- output capture limits and child timeout termination
- platform fixture: 20 fixed tests + 10 learner tests + CLI smoke test (31 total)
- prerequisite: current PASS for W4 quality gate

## P4 — W6: concurrency and thread pool

**Status: merged into `main` via PR #9; hosted Ubuntu 24.04 CI passed.**

- fixed worker pool with bounded queue backpressure
- futures/promises, move-only callable/argument support and exception propagation
- linearizable submit/shutdown boundary and once-only concurrent joining
- platform fixture: 23 fixed tests + 10 learner tests + CLI smoke (34 total); final GCC, ASan/UBSan, clang-format and clang-tidy CI passed
- prerequisite: current PASS for W5 system programming lab


- POSIX file descriptors and system calls
- processes and lifecycle
- std::thread, mutex, condition_variable
- producer/consumer queue
- thread pool, futures, templates, exception and shutdown semantics
- atomics, tests, benchmarks, logging

## P5 — W7: TCP/IP and socket programming

**Status: W7-A and W7-B are both merged into `main` via PRs #10 and #11; hosted Ubuntu CI passed.**

- W7-A: IPv4 socket API, blocking TCP echo server/client, partial sends, descriptor lifetime
- W7-B: thread-per-connection learning path, non-blocking sockets, select/poll, epoll
- final work product: multi-client TCP chat with connection state, newline framing, read/write buffers and input limits
- W7-A acceptance: 21 fixed tests + 10 learner tests + CLI smoke (32 total); hosted GCC, ASan/UBSan, clang-format and clang-tidy CI passed

### P5-B — W7 Multi-client TCP Chat with epoll

**Status: merged into `main` via PR #11; hosted Ubuntu 24.04 CI passed.**

- thread-per-connection, non-blocking I/O, select/poll and epoll comparison
- final level-triggered epoll chat server with single-owner connection state
- nickname protocol, message broadcast and newline framing
- bounded input/output buffers, slow-client handling and eventfd stop wakeup
- acceptance: 26 platform tests + 10 learner tests + CLI smoke (37 total)
- prerequisite: current PASS for W7-A TCP Echo

## P6 — W8: HTTP server engineering

**Status: W8-D1–D6-D3 merged into `main`; six D6 learner tasks are being registered on `feat/w8-advanced-taskization`.**

- D1: HTTP/1.1 message model, request-line parser and response serializer; 33 fixed acceptance checks passed.
- D2: incremental parser, NeedMoreData/Complete/Error states, strict header syntax, Content-Length and byte-accurate consumed length; 39 checks passed.
- D3: response serialization, binary static-file reads, MIME types, 400/404/405 handling and conservative path checks; 31 checks passed.
- D4: method + path Router, dynamic Hello/Echo/Health handlers, 405 Allow, static fallback and byte-exact CLI integration; 43 checks passed.
- D5: non-blocking TCP listener, single-owner epoll Reactor, W6 ThreadPool handoff, completion queue/eventfd, generation-safe stale completion rejection, bounded buffers and 400/413/500/503 paths. Merged as `f503d3f`; final-head Ubuntu 24.04 run `42956321926` passed all three jobs. Acceptance: 23 platform tests + 10 reference tests + CLI loopback smoke (34 total); root GCC/CTest and ASan/UBSan each 53/53; Router fixture 43/43; formatting and clang-tidy passed. Windows 11 + WSL2 target acceptance remains separate.
- D6-A: merged as `241059c`; explicit HTTP/1.1/1.0 connection policy, exact case-insensitive Connection token parsing, authoritative response Connection header, and backward-compatible close-only serializer overload. Both maintained protocol copies passed 14 tests each under GCC, ASan/UBSan and clang-tidy.
- D6-B: merged as `8597a38` (PR #21); sequential Keep-Alive lifecycle, buffered subsequent requests, HTTP/1.0 response-version preservation, peer half-close pipeline draining and strict response ordering. Final-head run `42971917343` passed GCC, ASan/UBSan, clang-format and clang-tidy; loopback fixture 8/8.
- D6-C: merged as `974b3d2` (PR #22); timerfd-backed absolute idle/header/body deadlines, HTTP 408 for incomplete requests, per-connection request cap and persistent-connection quota. Final-head run `42977367404` passed all three jobs; the nine-case loopback fixture passed under GCC, ASan/UBSan and clang-tidy.
- D6-D1: merged as `d29d38e` (PR #23); stop accepting, close idle clients, return 503 for incomplete requests, drain in-flight handlers with Connection: close, and enforce a monotonic socket-drain deadline. Final-head run `42979170395` passed all three jobs; fixture 5/5 under GCC, ASan/UBSan and clang-tidy.
- D6-D2: merged as `546f87c` (PR #24); JSONL access/error/lifecycle events, query redaction, custom log sink, atomic runtime metrics and operational gauges. Final-head run `42986255957` passed all three jobs; eight-case observability fixture passed under GCC, ASan/UBSan and clang-tidy.
- D6-D3: merged as `597a8a0` (PR #25); four-case fixed loopback fixture for 16-client concurrent load and worker bounds, 8 MiB slow-reader backpressure, 320-request Keep-Alive soak, and eight repeated server lifecycle cycles. Final tested head `736a0ac`; run `42991542424` passed all three jobs; D6-D3 fixture 4/4 under GCC, ASan/UBSan and clang-tidy.

### W8-D6 advanced tutorial taskization

**Merged into `main` via PR #26 (`c8f8e03`).** The formal catalog now contains 20 tasks with a strict D6-A → D6-B → D6-C → D6-D1 → D6-D2 → D6-D3 prerequisite chain. Each task has a focused learner source boundary, three progressive hints and a fixed fixture that builds the learner's declared implementation. D6-A through D6-D2 use staged previous-milestone server baselines; D6-D3 implements a reusable HTTP load-probe client while retaining the existing server operational regression suite. Final PR-head run `42998617546` passed all three jobs: root 60/60 and task-mode 49/49 under GCC, ASan/UBSan and clang-tidy. The end-to-end workflow test invokes `cppapplab check` for all six tasks and verifies current PASS evidence.

- TCP/IP and Linux socket API
- blocking and nonblocking I/O
- select, poll, epoll
- multi-client TCP chat
- HTTP/1.1 parsing, framing, response serialization
- router, static files, event loop + worker pool
- keep-alive, limits, error handling, logging, benchmark, release

**Learner usability milestones merged:** PR #30 (`917b45c`) introduced the guided `learn` workflow and one-command Ubuntu setup; PR #31 (`89ea327`) added the interactive home and actionable check feedback; PR #32 (`991007e`) added task-title search and granular task-state visibility. Final PR #32 CI run `43024884781` passed all three jobs, including root 75/75 under GCC and ASan/UBSan and the configured clang-format/clang-tidy and W8/W9 fixture gates.

**Windows launcher merged via PR #33 (986e9ff).** The Windows entry point detects WSL2 Ubuntu 24.04, prepares the Linux checkout on first launch, opens the interactive learner home on subsequent launches and provides an explicit rebuild mode. Final CI run 43031051999 passed all four jobs, including the Windows PowerShell 5.1/7 syntax gate and Bash bootstrap integration tests. Real WSL2 target-machine acceptance remains a release qualification item.

## P7 — Delivery and stabilization

- [ ] source package and learner package
- [ ] offline dependency strategy and reproducible checks
- [x] interactive learner home and numbered task selection
- [x] title-keyword task search and granular task-state visibility
- [x] Windows double-click launcher for the WSL2 Ubuntu 24.04 workflow
- [x] actionable check-failure diagnostics and retry guidance
- [x] first-use guide and troubleshooting
- [ ] Linux/WSL2 end-to-end verification
- [ ] version tag and release notes
- [ ] known limitations and evidence archive


## Next curriculum expansion — W9 to W12

**W9-A1 merged into `main` via PR #27 (`bfb3f3f`).** The task, `w9.http-chunked`, extends the W8 parser with bounded incremental HTTP/1.1 chunked decoding, trailer validation, anti-ambiguity framing and pipeline-safe consumed-byte reporting. Final PR-head run `43006252261` passed all three jobs: root 62/62 and W9 fixture 35/35 under GCC, ASan/UBSan and clang-tidy. The end-to-end learning-engine check passed. W9-A2 (strict request-target/query parsing) merged into main via PR #28 (`cca1c80`). Final PR-head run `43008862704` passed all three jobs: root 64/64, W9-A1 35/35 and W9-A2 39/39 under GCC, ASan/UBSan and clang-tidy. The end-to-end learning-engine checks passed for both W9 tasks. W9-A3 (`w9.static-root`) merged via PR #29 (`f741e34`). Final PR-head run `43013017702` passed all three jobs: root 66/66 and W9-A3 34/34 under GCC, ASan/UBSan and clang-tidy; W9-A1 and A2 regression fixtures passed 35/35 and 39/39. The end-to-end `cppapplab check` workflow verified current PASS evidence. W9-A4 and A5 remain roadmap stages.

The rest of W9 is planned as a cumulative secure-protocol sequence:

- **A2 — Request target and query parsing:** origin-form parsing, strict percent decoding, malformed-escape and control-byte rejection, query multimap.
- **A3 — Secure static root:** directory-fd confinement, openat component traversal, O_NOFOLLOW, regular-file verification and symlink escape tests (merged and validated).
- **A4 — HTTP hardening:** message-framing policy, request-smuggling regressions, resource limits and deterministic protocol errors.
- **A5 — Parser fuzzing:** sanitizer-backed fuzz target, seed corpus, regression cases and minimized reproductions.

Beyond W9:

- **W10 — TLS and WebSocket:** OpenSSL lifecycle, non-blocking TLS in epoll, WANT_READ/WANT_WRITE state transitions, HTTP Upgrade, WebSocket frame parsing and close/ping/pong.
- **W11 — High-performance C++ services:** profiling, std::pmr, buffer reuse, scatter/gather I/O, sendfile, batching, backpressure, cache locality and latency percentiles.
- **W12 — Production operations:** typed configuration, signal-driven shutdown/reload, health endpoints, correlation IDs, systemd deployment, recovery and release runbooks.

W9-A1–A3 are merged and validated. W9-A4–A5 and W10–W12 remain curriculum targets, not registered learner tasks; each will be added only after its learner source boundary, reference implementation and platform-owned acceptance fixture are ready.

The roadmap is outcome-based. Additional lessons are added only when they improve a demonstrated learning or engineering outcome.
