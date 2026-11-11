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

## W6 — Concurrency and thread pools

**Topics**

- std::thread worker lifecycle
- bounded task submission and backpressure
- std::packaged_task, std::future and asynchronous results
- callable/argument forwarding and move-only values
- exception propagation through futures
- submit/shutdown linearization and concurrent shutdown
- graceful queue drain and deterministic worker joining

**Work product:** a fixed-size C++17 thread pool built on the W5 bounded queue.

**Acceptance boundary:** 22 platform-owned behavior tests, 10 learner-authored tests and one CLI smoke test (33 total).

## W7 — TCP/IP and socket programming

**Topics**

- TCP/IP layers, IPv4 addresses, ports and network byte order
- Linux socket API: `socket/bind/listen/accept/connect/send/recv`
- blocking TCP echo server and client
- thread-per-connection and connection ownership
- nonblocking sockets and `EAGAIN/EWOULDBLOCK`
- `select`, `poll`, and `epoll`
- connection state, read/write buffering, partial I/O and TCP stream framing
- multi-client chat protocol, input limits and disconnect handling

**Work products:**
1. `w7.tcp-echo` — blocking TCP echo server/client, complete writes and RAII descriptor ownership. This task depends on W6 and is merged into `main`.
2. `w7.tcp-chat` — a multi-client Linux `epoll` chat server with non-blocking sockets, connection state, nickname registration, newline framing, buffered partial writes and graceful disconnects. This is the W7 completion target and depends on a current W7-A PASS.

## W8 — HTTP server engineering

**Topics**

- HTTP/1.1 request/response format, headers, status codes and CRLF framing
- request-line parsing and response serialization
- incremental request parser, headers and Content-Length handling
- static file serving, router and dynamic endpoints
- event-loop ownership and worker-pool integration
- keep-alive, timeouts, limits, logging and error responses
- release build, functional smoke tests and benchmark

**Work products:**
1. `w8.http-protocol` — HTTP message types, request-line parsing and response serialization. This D1 task depends on a current W7 TCP chat PASS.
2. `w8.http-parser` — incremental request parser with request line, case-insensitive headers, Content-Length and NeedMoreData/Complete/Error states. This D2 task depends on W8-D1 and reports consumed bytes for pipelined requests.
3. `w8.static-files` — response serialization integrated with a GET-only static-file handler, MIME types, 404/405 responses and conservative path-traversal rejection. This D3 task depends on W8-D2.
4. `w8.router` — method + path routing and dynamic endpoints, then integration with static-file fallback. This D4 task follows W8-D3.
5. `w8.http-server` — connect the parser, router, handler and serializer to a non-blocking TCP/epoll Reactor, bounded W6 ThreadPool, completion queue and eventfd wakeup. The D5 learner task establishes the real server and bounded dispatch path. The integrated reference server has since been extended through D6-A–D3 with sequential Keep-Alive, connection deadlines and quotas, graceful shutdown, observability, and operational load/soak validation.


### W8 advanced engineering extension — D6 connection lifecycle and operations

The D6 stages extend the same reference server beyond the five registered W8 learner tasks:

1. **D6-A — Connection policy:** HTTP/1.0 and HTTP/1.1 persistence rules and authoritative `Connection` response semantics. See [connection policy](lessons/w8/connection-policy.md).
2. **D6-B — Sequential Keep-Alive:** persistent connections, pipelined request buffering, response ordering and peer half-close draining. See [Keep-Alive lifecycle](lessons/w8/keep-alive.md).
3. **D6-C — Deadlines and quotas:** idle/header/body deadlines, request-count limits and persistent-connection quotas. See [connection guards](lessons/w8/connection-guards.md).
4. **D6-D1 — Graceful shutdown:** stop accepting, drain in-flight work and enforce a bounded socket-drain deadline. See [graceful shutdown](lessons/w8/graceful-shutdown.md).
5. **D6-D2 — Observability:** structured JSONL events, sensitive-query redaction and thread-safe runtime metrics. See [observability](lessons/w8/observability.md).
6. **D6-D3 — Operational validation:** concurrent clients, slow-reader TCP backpressure, Keep-Alive soak and repeated server lifecycle tests. See [operational load](lessons/w8/operational-load.md).

These extensions are now formal learner tasks in `course/tasks.tsv`, following `w8.http-server` in a strict prerequisite chain. Each task has its own learner workspace, three progressive hints and a platform-owned fixture that compiles the learner's declared source files.

| Task ID | Learner implementation | Acceptance |
|---|---|---:|
| `w8.http-connection-policy` | `http_message.hpp/.cpp`: connection policy and serializer overloads | 14 tests |
| `w8.http-keep-alive` | `http_server.hpp/.cpp`: sequential persistent-connection lifecycle | 8 tests |
| `w8.http-connection-guards` | `http_server.hpp/.cpp`: deadlines, quotas and timerfd scheduler | 9 tests |
| `w8.http-graceful-shutdown` | `http_server.hpp/.cpp`: bounded Reactor shutdown | 5 tests |
| `w8.http-observability` | `http_server.hpp/.cpp`: structured logs and runtime metrics | 8 tests |
| `w8.http-operational-load` | `http_load_probe.hpp/.cpp`: concurrent Keep-Alive probe and latency statistics | 5 tests |

D6-B through D6-D2 learner templates include a known completed baseline for the preceding stage, allowing each workspace to focus on one engineering delta while retaining a working server foundation. D6-D3's new load-probe task complements, rather than replaces, the existing four-case server-level operational regression fixture. The D6 task-mode CI uses the reference implementations only as known-good submissions to verify the learner acceptance contract; the learner templates themselves remain incomplete by design.

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
