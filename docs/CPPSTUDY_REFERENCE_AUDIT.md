# cppStudy Reference Audit

**Reviewed source:** `clonetimer/cppStudy`, default branch `main`, commit `55a950e715f1c10b418e2fa388019c6d4da21e11`.

## 1. Current project identity

The current README describes a Linux-only C++17 educational HTTP/1.0 and HTTP/1.1 server. The documented architecture includes:

- nonblocking TCP sockets and an `epoll` event loop
- incremental HTTP request parsing
- routing and static-file handling
- a worker thread pool
- completion queue and `eventfd` notification
- HTTP response serialization, keep-alive, limits, and access logging

The README explicitly positions the server as an educational implementation, not a production replacement for a mature HTTP server. It documents important limits including no TLS, HTTP/2/3, chunked transfer encoding, and production-grade logging.

## 2. Current CMake build boundary

The root `CMakeLists.txt` declares C++17 and CMake 3.18, builds:

- `http_core`: request parser, response, static-file handler, router, API handlers, thread pool, and logger
- `http_server`: the HTTP server executable
- `http_tests`: test sources selected from a fixed list

GoogleTest is pinned to v1.18.0 through CMake FetchContent when a system package is not found. ASan/UBSan is an optional build mode.

## 3. Test inventory discrepancy

The CMake test list names:

- `tests/http_request_parser_test.cpp`
- `tests/http_response_test.cpp`
- `tests/static_file_handler_test.cpp`
- `tests/router_test.cpp`

The inspected repository tree instead contains `tests/route_test.cpp` and `tests/static_file_hander_text.cpp`; the two names expected by CMake are absent. Because the CMake file wraps each source in `if(EXISTS)`, those two test targets are silently omitted rather than causing configuration to fail.

The tree also contains other tests such as `thread_pool_test.cpp`, `task_test.cpp`, `task_manager_test.cpp`, and `thread_safe_queue_test.cpp`. They are not in the current HTTP test source list and must not be counted as part of the current HTTP CI coverage without a separate build/test invocation.

**Conclusion:** a green root CMake/CTest run does not establish that every test file in cppStudy is compiled or passing.

## 4. Reuse policy for CppAppLab

cppStudy is a valuable source of real engineering examples, but its contents must be migrated selectively:

1. W1/W2: inspect the calculator and Todo code as early project references; add independent learner templates and platform-owned tests.
2. W5/W6: audit the thread-safe queue and thread-pool implementations, their ownership rules, exception paths, shutdown semantics, and test coverage before using them as reference solutions.
3. W7: reuse the TCP/socket concepts and verified server patterns, not unverified historical variants.
4. W8: use the current HTTP server as a high-value reference for parser, router, event-loop, worker-pool, and connection-ownership topics.

For every migrated project, CppAppLab must own its own build fixture and acceptance tests. The learner's CMakeLists.txt and tests are not the acceptance authority.

## 5. Open audit items

- Reconcile the two missing HTTP test filenames and decide whether the current tests are obsolete, renamed, or simply not wired.
- Determine which historical server implementations (`server.cpp`, `server_poll.cpp`, `server_select.cpp`, `http_server.cpp`) are intended reference stages.
- Review the thread-pool and thread-safe-queue tests against the current implementations before declaring those components accepted.
- Run a clean Debug, Release, and ASan/UBSan build on the target Ubuntu environment before using cppStudy as a frozen W7/W8 reference baseline.
