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

- CLI: `list/start/lesson/hint/check/status/next`
- task registry and learner workspace marker
- reference and learner source separation
- explicit-only execution; navigation commands do not execute learner code
- fixed GCC compile + exact-output acceptance
- source fingerprint and stale local evidence handling
- symlink/path-traversal rejection for declared source paths
- process timeout, output cap, stdin isolation, and process-group termination

Hosted Ubuntu CI passed 15/15 tests in both ordinary and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

### P1-B — W1 multi-file Calculator

**Status: implemented on `feat/p1b-calculator`; final hosted CI passed; PR pending review and merge.**

- second registered learning task
- separate interface, implementation, and command-line front end
- platform-owned CMake/GoogleTest acceptance fixture
- seven fixed arithmetic/error-path tests
- CLI smoke checks for normal arithmetic and division by zero
- 17/17 hosted GoogleTest cases in both ordinary and ASan/UBSan builds
- source staging so learner-controlled build files and headers cannot replace the platform fixture

### Remaining P1 work

- prerequisite graph and cross-workspace progress
- more complete diagnostics and task-specific acceptance runners
- workspace recovery and evidence history
- task catalog externalization

**P1 exit criteria:** a learner can start a task, read it, request hints, implement it, run fixed platform-owned checks, inspect failures, and resume without losing work. A local evidence record is navigation data, not a certificate.

## P2 — W1 to W3: C++ foundations

- W1: toolchain, syntax, functions, multi-file compilation, CMake, GDB, calculator
- W2: STL, algorithms, lambdas, file persistence, Todo CLI
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
