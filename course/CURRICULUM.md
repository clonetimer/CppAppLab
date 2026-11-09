# CppAppLab Curriculum Map

## Curriculum principle

The course follows the eight-week sequence in the supplied *快速C++* learning material. The cumulative work products are kept in the same order so that later lessons reuse earlier code rather than replacing it with unrelated demonstrations.

The first-version language baseline is C++17. WSL2 Ubuntu 24.04 is the full acceptance environment.

## W1 — Toolchain and first programs

**Topics**

- WSL2 and Ubuntu setup
- GCC/g++, CMake, Git, GDB, VS Code Remote WSL
- Hello World and command-line compilation
- functions, input/output, basic types and control flow
- multi-file compilation, headers, declarations and definitions
- CMake configure/build/link
- GDB breakpoints, stepping, variables, and backtrace
- error handling and a loop-based command-line calculator

**Work product:** a multi-file calculator built with CMake and debugged with GDB.

## W2 — STL and data persistence

**Topics**

- `std::string` and `std::string_view`
- `std::vector`, `std::array`, iterators and range-for
- `std::map`, `std::unordered_map`, `std::set`
- standard algorithms and lambdas
- `std::fstream`, file errors, serialization and deserialization

**Work product:** a Todo CLI supporting add/list/done/remove and file persistence.

## W3 — Classes and resource lifetime

**Topics**

- `struct` / `class`, access control, constructors and destructors
- RAII, stack and heap lifetime
- `unique_ptr`, `shared_ptr`, `weak_ptr`
- copy constructor/assignment and move constructor/assignment
- operator overloads
- encapsulation and ownership boundaries

**Work product:** refactor Todo into `Task` and `TaskManager`, with explicit resource ownership.

## W4 — Testing and maintainable C++

**Topics**

- GoogleTest and CTest
- normal, boundary, and error-path tests
- GDB with failing tests, conditional breakpoints, watchpoints and call stacks
- AddressSanitizer, UndefinedBehaviorSanitizer, Valgrind
- compiler warnings, clang-format and clang-tidy
- README, GitHub Actions CI, clean build, version tag and release notes

**Work product:** a tested and reproducibly buildable Todo CLI release candidate.

## W5 — Linux system programming

**Topics**

- POSIX file descriptors and `open/read/write/close`
- process creation and lifecycle: `fork/exec/wait`
- `std::thread`, join and detach
- `std::mutex`, `lock_guard`, `unique_lock`
- `std::condition_variable`
- producer/consumer model and thread-safe queue

**Work product:** a thread-safe queue and producer/consumer demonstration using real Linux process/file interfaces where specified.

## W6 — Concurrency engineering

**Topics**

- task queue and worker threads
- `std::future`, `std::promise`, `std::packaged_task`
- generic submit, variadic templates and perfect forwarding
- exception propagation, shutdown semantics and rejecting new work
- `std::atomic` and memory-order fundamentals
- concurrency tests, logging and benchmarking

**Work product:** a reusable thread pool with defined submission, exception, and shutdown behavior.

## W7 — TCP/IP and socket programming

**Topics**

- TCP/IP layers and Linux socket API
- blocking TCP server
- nonblocking sockets and `EAGAIN/EWOULDBLOCK`
- `select`, `poll`, and `epoll`
- multiple clients, connection lifecycle, framing and error handling

**Work product:** a multi-client TCP chat server.

## W8 — HTTP server engineering

**Topics**

- HTTP request/response format, headers, status codes and framing
- incremental request parser and Content-Length handling
- response serialization and static file serving
- router and dynamic endpoints
- event-loop ownership and worker-pool integration
- keep-alive, timeouts, limits, logging and error responses
- release build, functional smoke tests and benchmark

**Work product:** an educational HTTP/1.1 server built on the previous TCP and concurrency work.

## Learning evidence

For every work product, the future task catalog must define:
- prerequisite tasks and expected knowledge
- files and interfaces the learner implements
- fixed platform-owned behavior tests
- boundary and error-path tests
- progressive hints
- build/test commands
- an explanation or design question

Passing an automated task proves the tested behavior for the recorded source state. It does not by itself prove independent mastery.
