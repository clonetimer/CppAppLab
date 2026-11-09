# CppAppLab

**CppAppLab** is a hands-on C++ learning and engineering-practice platform. It is designed to take a learner from the first C++ program to maintainable Linux systems software, concurrent programs, TCP services, and an HTTP/1.1 server.

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

## Learning path

| Stage | Focus | Main work product |
|---|---|---|
| W1 | Environment, C++ basics, multi-file compilation, CMake, GDB | Command-line calculator |
| W2 | STL, algorithms, lambdas, file I/O | Persistent Todo CLI |
| W3 | Classes, RAII, smart pointers, copy/move semantics | Refactored Task / TaskManager |
| W4 | GoogleTest, debugging, sanitizers, static analysis, CI | Tested and versioned C++ project |
| W5 | Linux file APIs, processes, threads, synchronization | Thread-safe queue and producer/consumer demo |
| W6 | Thread pools, futures, templates, atomics, benchmarking | Reusable thread pool |
| W7 | TCP/IP, sockets, nonblocking I/O, select/poll/epoll | Multi-client TCP chat |
| W8 | HTTP/1.1, parser, response, routing, static files, event loop | Educational HTTP server |

The detailed course map and acceptance boundaries live in [course/CURRICULUM.md](course/CURRICULUM.md).

## Current learning engine

The first usable vertical slice is W1 Hello World. Build the platform, then use:

```bash
./build/cppapplab list
./build/cppapplab start w1.hello-world "$HOME/cppapplab-work/hello-world"
./build/cppapplab lesson "$HOME/cppapplab-work/hello-world"
./build/cppapplab hint "$HOME/cppapplab-work/hello-world" 1
./build/cppapplab check "$HOME/cppapplab-work/hello-world"
./build/cppapplab status "$HOME/cppapplab-work/hello-world"
```

The learning engine design and its evidence boundaries are in [docs/LEARNING_ENGINE_DESIGN.md](docs/LEARNING_ENGINE_DESIGN.md). The W1 Calculator task is the next implementation increment, not yet available.

## Project principles

1. **Real programs before isolated syntax drills.** Each concept is introduced in the context of a working program.
2. **Reference and learner work are separate.** Reference solutions are not used as learner submissions.
3. **Fixed acceptance contracts.** Platform-owned tests validate behavior; learner tests and build files are not treated as the authority for acceptance.
4. **Hints do not replace implementation.** Learners can request progressive hints, implement the task, and rerun checks.
5. **Evidence is reproducible.** A check records compiler output, toolchain metadata, source fingerprint, and exit status.
6. **No fake portability.** POSIX and Linux networking lessons use the actual Linux APIs they teach.
7. **CI is not human learning validation.** Green automation proves build/test behavior, not that a learner understands the code.

## Repository status

The repository is in P1 learning-engine development. See:
- [Architecture](docs/ARCHITECTURE.md)
- [Learning engine design](docs/LEARNING_ENGINE_DESIGN.md)
- [Roadmap](docs/ROADMAP.md)
- [Development and acceptance](docs/DEVELOPMENT.md)
- [Environment setup](docs/ENVIRONMENT_WSL2_UBUNTU.md)
- [Project status](docs/STATUS.md)

## Quick start

```bash
git clone https://github.com/clonetimer/CppAppLab.git
cd CppAppLab

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/cppapplab --help
```

## Scope boundary

CppAppLab is an educational engineering lab, not a production C++ framework or a substitute for official compiler, CMake, Linux, or protocol documentation. Course acceptance focuses on correctness, resource lifetime, error handling, reproducible builds, tests, and explainable design decisions.
