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
- W3: classes, RAII, smart pointers, copy/move, operators, TaskManager refactor (**P2-B in progress on `feat/p2-w3-task-manager`**)

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

**Status: implementation complete on `feat/p3-w4-quality-gate`; hosted CI passed; PR preparation pending.**

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

**Status: W7-A TCP Echo Server merged into `main` via PR #10; W7-B epoll chat server is next.**

- W7-A: IPv4 socket API, blocking TCP echo server/client, partial sends, descriptor lifetime
- W7-B: thread-per-connection learning path, non-blocking sockets, select/poll, epoll
- final work product: multi-client TCP chat with connection state, newline framing, read/write buffers and input limits
- W7-A acceptance: 21 fixed tests + 10 learner tests + CLI smoke (32 total); hosted GCC, ASan/UBSan, clang-format and clang-tidy CI passed

## P6 — W8: HTTP server engineering

- TCP/IP and Linux socket API
- blocking and nonblocking I/O
- select, poll, epoll
- multi-client TCP chat
- HTTP/1.1 parsing, framing, response serialization
- router, static files, event loop + worker pool
- keep-alive, limits, error handling, logging, benchmark, release

## P7 — Delivery and stabilization

- source package and learner package
- offline dependency strategy and reproducible checks
- first-use guide and troubleshooting
- Linux/WSL2 end-to-end verification
- version tag and release notes
- known limitations and evidence archive

The roadmap is outcome-based. Additional lessons are added only when they improve a demonstrated learning or engineering outcome.
