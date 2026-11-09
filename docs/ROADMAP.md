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

**Status: implementation in progress on `feat/p2-w2-todo-cli`; hosted CI pending.**

- STL-backed Todo model with `std::vector`, standard algorithms, and `std::unordered_map`
- add/list/done/remove CLI commands
- versioned text persistence with escaped title fields
- load validates into a temporary collection before replacing current data
- save publishes a complete temporary file by rename
- platform-owned CMake/GoogleTest fixture with 18 unit tests and one multi-process CLI workflow test
- task depends on a current W1 Calculator PASS

**P1 exit criteria:** a learner can start a task, read it, request hints, implement it, run fixed platform-owned checks, inspect failures, and resume without losing work. A local evidence record is navigation data, not a certificate.

## P2 — W1 to W3: C++ foundations

- W1: toolchain, syntax, functions, multi-file compilation, CMake, GDB, calculator
- W2: STL, algorithms, lambdas, file persistence, Todo CLI (P2-A)
- W3: classes, RAII, smart pointers, copy/move, operators, TaskManager refactor

## P3 — W4: engineering quality

- GoogleTest design and boundary/error-path coverage
- GDB with tests
- ASan / UBSan and Valgrind
- compiler warnings, clang-format, clang-tidy
- GitHub Actions, versioning, release notes

## P4 — W5 to W6: Linux and concurrency

- POSIX file descriptors and system calls
- processes and lifecycle
- std::thread, mutex, condition_variable
- producer/consumer queue
- thread pool, futures, templates, exception and shutdown semantics
- atomics, tests, benchmarks, logging

## P5 — W7 to W8: network engineering

- TCP/IP and Linux socket API
- blocking and nonblocking I/O
- select, poll, epoll
- multi-client TCP chat
- HTTP/1.1 parsing, framing, response serialization
- router, static files, event loop + worker pool
- keep-alive, limits, error handling, logging, benchmark, release

## P6 — Delivery and stabilization

- source package and learner package
- offline dependency strategy and reproducible checks
- first-use guide and troubleshooting
- Linux/WSL2 end-to-end verification
- version tag and release notes
- known limitations and evidence archive

The roadmap is outcome-based. Additional lessons are added only when they improve a demonstrated learning or engineering outcome.
