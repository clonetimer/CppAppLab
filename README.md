# CppAppLab

**CppAppLab** is a hands-on C++ learning and engineering-practice platform. It takes a learner from the first C++ program to maintainable Linux systems software, concurrent programs, TCP services, and an HTTP/1.1 server.

## First-version baseline

- **Host:** Windows 11 with WSL2
- **Linux:** Ubuntu 24.04 LTS
- **Language:** C++17
- **Compiler:** GCC / g++
- **Build:** CMake
- **Debugger:** GDB
- **Version control:** Git and GitHub
- **Tests:** GoogleTest and CTest
- **Quality:** compiler warnings, clang-format, clang-tidy, ASan and UBSan
- **CI:** GitHub Actions on Ubuntu

Linux/POSIX topics use real Linux APIs. The first release does not claim full native MSVC parity; Windows-native compatibility can be added for portable course modules after the Linux baseline is stable.

## Current learning tasks

The learning engine provides W1 Hello World and Calculator, W2 persistent Todo CLI, W3 TaskManager refactor, W4 quality gate, W5 Linux systems, W6 thread pool, W7 TCP Echo and epoll chat, and W8 HTTP protocol, incremental parser, static files, Router/dynamic handlers, and the W8-D5 TCP/epoll HTTP server. W8-D6-A connection policy is merged; D6-B sequential Keep-Alive has passed hosted CI and is awaiting merge. The catalog is the authoritative task list.

```bash
./build/cppapplab list

./build/cppapplab start w1.hello-world "$HOME/cppapplab-work/hello-world"
./build/cppapplab lesson "$HOME/cppapplab-work/hello-world"
./build/cppapplab hint "$HOME/cppapplab-work/hello-world" 1
./build/cppapplab check "$HOME/cppapplab-work/hello-world"

./build/cppapplab start w1.calculator "$HOME/cppapplab-work/calculator"
./build/cppapplab lesson "$HOME/cppapplab-work/calculator"
./build/cppapplab hint "$HOME/cppapplab-work/calculator" 1
./build/cppapplab check "$HOME/cppapplab-work/calculator"

./build/cppapplab start w2.todo-cli "$HOME/cppapplab-work/todo"
./build/cppapplab lesson "$HOME/cppapplab-work/todo"
./build/cppapplab hint "$HOME/cppapplab-work/todo" 1
./build/cppapplab check "$HOME/cppapplab-work/todo"

./build/cppapplab start w3.task-manager "$HOME/cppapplab-work/task-manager"
./build/cppapplab lesson "$HOME/cppapplab-work/task-manager"
./build/cppapplab hint "$HOME/cppapplab-work/task-manager" 1
./build/cppapplab check "$HOME/cppapplab-work/task-manager"

./build/cppapplab start w4.quality-gate "$HOME/cppapplab-work/quality-gate"
./build/cppapplab lesson "$HOME/cppapplab-work/quality-gate"
./build/cppapplab hint "$HOME/cppapplab-work/quality-gate" 1
./build/cppapplab check "$HOME/cppapplab-work/quality-gate"

./build/cppapplab progress
./build/cppapplab resume w1.hello-world
./build/cppapplab history w1.hello-world
```

The Calculator acceptance runner uses a platform-owned CMake/GoogleTest fixture. It does not trust learner-provided CMakeLists.txt or tests.

## Course progress and workspace recovery

- `cppapplab progress` shows task states and prerequisite locks.
- `cppapplab resume <task-id>` reopens the most recently started workspace for that task, including before its first check.
- `cppapplab history <task-id>` shows previous acceptance attempts.
- A dependent task can start after its prerequisite has a current PASS in any saved workspace.
- A PASS is current only while its workspace exists and the declared source fingerprint still matches.

Progress is stored outside the repository by default:

```text
~/.local/share/cppapplab/progress/
```

Set `CPPAPPLAB_STATE_DIR` to move this state directory. The course catalog is maintained separately in `course/tasks.tsv`; it declares lesson paths, source files, acceptance type, and prerequisite relationships.

## Learning path

| Stage | Focus | Main work product |
|---|---|---|
| W1 | Environment, C++ basics, multi-file compilation, CMake, GDB | Calculator |
| W2 | STL, algorithms, lambdas, file I/O | Persistent Todo CLI |
| W3 | Classes, RAII, smart pointers, copy/move semantics | Refactored Task / TaskManager |
| W4 | GoogleTest, debugging, sanitizers, static analysis, CI | Tested and versioned C++ project |
| W5 | Linux file APIs, processes, threads, synchronization | Thread-safe queue and producer/consumer demo |
| W6 | Thread pools, futures, templates, atomics, benchmarking | Reusable thread pool |
| W7 | TCP/IP, sockets, nonblocking I/O, select/poll/epoll | Multi-client TCP chat |
| W8 | HTTP/1.1, parser, response, routing, static files, event loop | Educational HTTP server |

The detailed course map and acceptance boundaries live in [course/CURRICULUM.md](course/CURRICULUM.md). W2's Todo task exercises STL algorithms and durable file I/O through a cumulative command-line application.

## Project principles

1. **Real programs before isolated syntax drills.** Each concept is introduced in the context of a working program.
2. **Reference and learner work are separate.** Reference solutions are not used as learner submissions.
3. **Fixed acceptance contracts.** Platform-owned tests validate behavior; learner tests and build files are not the authority for acceptance.
4. **Hints do not replace implementation.** Learners can request progressive hints, implement the task, and rerun checks.
5. **Evidence is reproducible.** A check records compiler output, toolchain metadata, source fingerprint, and exit status.
6. **No fake portability.** POSIX and Linux networking lessons use the actual Linux APIs they teach.
7. **CI is not human learning validation.** Green automation proves build/test behavior, not that a learner understands the code.
8. **Progress survives sessions.** Workspace locations and check history are stored outside the source checkout; current PASS state is revalidated against the workspace source fingerprint.

## Repository status

W1–W7 and W8-D1–D6-D1 are merged into `main`. W8-D5 adds a real TCP listener and epoll/worker-pool handoff; D6-A defines connection policy, D6-B sequential Keep-Alive, D6-C deadlines/quotas and D6-D1 graceful shutdown. D6-D2 structured logs/metrics are in progress; D6-D3 load/soak and Windows 11 + WSL2 target acceptance remain. No release or target WSL2 acceptance has been declared. See:
- [Architecture](docs/ARCHITECTURE.md)
- [Learning engine design](docs/LEARNING_ENGINE_DESIGN.md)
- [Roadmap](docs/ROADMAP.md)
- [Development and acceptance](docs/DEVELOPMENT.md)
- [Environment setup](docs/ENVIRONMENT_WSL2_UBUNTU.md)
- [Current project status](docs/STATUS.md)
- [Router lesson and exact task boundary](course/lessons/w8/router.md)
- [HTTP TCP server lesson](course/lessons/w8/http-server.md)
- [HTTP connection policy lesson](course/lessons/w8/connection-policy.md)
- [HTTP Keep-Alive lifecycle lesson](course/lessons/w8/keep-alive.md)
- [HTTP connection guards lesson](course/lessons/w8/connection-guards.md)
- [HTTP graceful shutdown lesson](course/lessons/w8/graceful-shutdown.md)
- [HTTP observability lesson](course/lessons/w8/observability.md)

## Quick start

```bash
git clone https://github.com/clonetimer/CppAppLab.git
cd CppAppLab

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/cppapplab --help
```

After satisfying the static-files prerequisite, start `w8.router` like any other learning task. For a reference-only demonstration:

```bash
cmake -S labs/w8/router/reference -B build-router
cmake --build build-router --parallel
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build-router/http-router parse-stdin
```

The Router CLI is a one-request application-layer demonstration. The W8-D5 server target binds a real TCP port; run `./build/http-server server 8080 www` to start it.

## Scope boundary

CppAppLab is an educational engineering lab, not a production C++ framework or a substitute for official compiler, CMake, Linux, or protocol documentation. Course acceptance focuses on correctness, resource lifetime, error handling, reproducible builds, tests, and explainable design decisions.
