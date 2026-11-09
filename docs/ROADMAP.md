# CppAppLab Roadmap

## P0 — Foundation

**Goal:** establish a reproducible project baseline and freeze the first-version boundary.

- Repository layout and engineering conventions
- WSL2 + Ubuntu 24.04 + GCC + CMake + GDB environment contract
- C++17 build target and minimal CLI
- GoogleTest / CTest baseline
- clang-format, clang-tidy, ASan, UBSan configuration
- GitHub Actions build, test, sanitizer, and quality jobs
- Course map and learning/acceptance contract

**Exit criteria:** clean configure/build/test on the target environment; CI green; architecture and curriculum boundaries documented.

## P1 — Learning engine

- CLI commands: \`start\`, \`lesson\`, \`hint\`, \`check\`, \`next\`, \`status\`
- Course/task catalog and prerequisite graph
- Learner workspace creation without overwriting existing work
- Fixed platform-owned acceptance fixtures
- Source fingerprint and stale-evidence handling
- Actionable diagnostics and bounded output/time
- Progress report and workspace recovery behavior

**Exit criteria:** a learner can start an exercise, read it, request hints, implement it, run a genuine check, inspect a failure, and resume without losing work.

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

The roadmap is intentionally outcome-based. Additional lessons are added only when they improve a demonstrated learning or engineering outcome.
