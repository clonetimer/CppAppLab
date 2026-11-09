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

**Implemented task:** `w2.todo-cli` (P2-A). The learner uses `std::vector`, standard algorithms, `std::unordered_map`, and versioned text serialization. The platform acceptance contract covers malformed input, escaped titles, file safety, and a multi-process command workflow.

## W3 — Classes, RAII, and ownership

**Topics**

- classes, access control, constructors, invariants, and encapsulation
- value semantics and object identity
- operator overloading for equality, ordering, and stream output
- RAII and deterministic resource lifetime
- `std::unique_ptr` and exclusive ownership
- non-owning observers versus owning handles
- deep copy, copy-and-swap, move construction, and move assignment
- preserving an existing serialization format during an internal refactor

**Work product:** refactor the W2 Todo core into a `Task` value object and an owning `TaskManager`, retaining the same CLI commands and W2 data-file compatibility.

**Acceptance boundary:** the learner implements both class source files. The platform supplies fixed headers and 30 unit tests plus one end-to-end CLI workflow test. The reference implementation is not copied into the learner workspace.

## W4 — Testing and maintainable C++

**Topics**

- GoogleTest / CTest and Arrange–Act–Assert
- normal, boundary, negative and failure-path tests
- GDB breakpoints, stepping, watchpoints and call-stack inspection
- AddressSanitizer and UndefinedBehaviorSanitizer
- clang-format and clang-tidy
- Valgrind leak diagnostics
- clean builds, reproducible quality scripts and engineering README

**Work product:** a tested and reproducibly buildable W3 TaskManager application with learner-authored tests and a documented quality gate.

**Acceptance boundary:** 30 platform-owned tests, 10 learner-authored tests and one multi-process CLI workflow test (41 total). Sanitizer and static-analysis builds are separate from the normal Debug build.

## W5 — Linux system programming

**Topics**

- POSIX file descriptors and `open/read/write/close`
- process creation and lifecycle: `fork/exec/wait`
- `std::thread`, join and detach
- `std::mutex`, `lock_guard`, `unique_lock`
- `std::condition_variable`
- producer/consumer model and thread-safe queue

**Work product:** a bounded thread-safe queue and a POSIX child-process runner that captures output, enforces a timeout, and reports process termination.

**Implemented task:** `w5.system-lab`. The learner implements queue synchronization and the process runner. The platform fixture covers 20 fixed tests, 10 learner-authored tests and one CLI smoke test (31 total).

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
