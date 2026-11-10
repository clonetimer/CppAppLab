# W8-D5 HTTP TCP Server Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Integrate the merged W8 HTTP application pipeline with a non-blocking TCP/epoll event loop and bounded worker execution.

**Architecture:** A single Reactor thread owns listener/client sockets, epoll registrations, connection state and buffers. It submits owned requests to the existing W6 ThreadPool; workers dispatch Router/static-file work and publish response completions through a mutex-protected queue plus eventfd. Generation tokens prevent stale events/completions from targeting reused file descriptors.

**Tech Stack:** C++17, Linux/POSIX sockets, epoll, eventfd, existing W6 ThreadPool, W8 parser/Router/static handler/serializer, CMake, GoogleTest, CTest, ASan/UBSan, clang-format, clang-tidy.

**Spec:** docs/designs/2026-11-10-w8-http-server.md and supplied 快速C++ D5 section.

## Global Constraints

- C++17; WSL2 Ubuntu 24.04 is the target acceptance environment.
- Preserve the merged W8-D1/D2/D3/D4 contracts and fixed acceptance fixtures.
- Linux networking uses real POSIX APIs; no simulated Python server.
- Platform-owned tests and CMake remain the acceptance authority.
- No Release or local Windows/WSL2 acceptance claim without evidence.

## Review Focus

- File-descriptor reuse and stale epoll/completion events: generation-token regression.
- Worker/event-loop data races: worker receives values only; no Connection/socket references.
- Event-loop blocking under worker queue pressure: maintain an outstanding-job counter that includes queued, running and undrained completions; reject with 503 before submit when it reaches task_queue_capacity.
- Partial TCP reads/writes and binary request bodies: fragmented input and large response tests.
- Shutdown/lifetime ordering: eventfd wake, drain/close connections, join workers before destroying shared state.

---

### Task 1: HTTP server API and fixed acceptance contract

**Files:**
- Create: labs/w8/http-server/learner/include/http_server.hpp
- Create: labs/w8/http-server/reference/include/http_server.hpp
- Create: course/fixtures/w8/http-server/include/http_server.hpp
- Create: course/fixtures/w8/http-server/tests/http_server_acceptance_test.cpp
- Create: tests/http_server_workflow_test.cpp
- Modify: course/tasks.tsv, CMakeLists.txt, tests/catalog_test.cpp

**Interfaces:**
- HttpServerConfig: bind address, port, document root, worker count, task queue capacity, maximum connections and buffer limits.
- HttpServer: non-copyable; port(), running(), blocking run(), thread-safe request_stop().
- Prerequisite: w8.router; task id w8.http-server, week 8, order 5.

- [ ] Add catalog and workflow regression tests first; observe the expected missing-task/fixture failure.
- [ ] Add platform-owned fixed fixture and reference-as-learner tests plus loopback integration tests.
- [ ] Wire the root workflow to run the real cppapplab check with learner CMake disabled.
- [ ] Verify root CTest and the fixed fixture expected count.

### Task 2: Reactor, worker handoff and bounded runtime

**Files:**
- Create: labs/w8/http-server/learner/src/http_server.cpp
- Create: labs/w8/http-server/reference/src/http_server.cpp
- Create: labs/w8/http-server/learner/src/main.cpp
- Create: labs/w8/http-server/reference/src/main.cpp
- Create: labs/w8/http-server/learner/CMakeLists.txt
- Create: labs/w8/http-server/reference/CMakeLists.txt
- Create: labs/w8/http-server/learner/README.md
- Create: labs/w8/http-server/reference/README.md
- Modify: .github/workflows/ci.yml, docs/STATUS.md, README.md

**Interfaces:**
- Reuse cppapplab::concurrency::ThreadPool from W6 and W8 HttpRequestParser, dispatch_request, serialize_response.
- Workers own request/response values only. Event loop owns all descriptors and connection state.
- Completion identity is (fd, generation); epoll event identity is the generation token.
- D5 preserves the current serializer Connection: close behavior: one request per TCP connection. Keep-alive/pipelining is D6.

- [ ] Implement listener setup, nonblocking accept, epoll registration and RAII descriptor cleanup.
- [ ] Implement bounded per-connection buffers, incremental parsing, 400/413 errors and one in-flight request.
- [ ] Implement worker dispatch, exception-to-500 mapping, completion queue and eventfd wakeup.
- [ ] Implement generation-safe completion lookup, EPOLLOUT-only-when-needed, partial send and close-after-write.
- [ ] Implement bounded overload/503 and eventfd shutdown.
- [ ] Add CLI http-server server <port> [document-root].
- [ ] Run native integration probes, full root CTest, fixed fixture, sanitizers, formatting and direct fixture clang-tidy.

### Task 3: Review, integration and status

- [ ] Review all changed files for ownership, limits, stale descriptors and shutdown races.
- [ ] Fix all critical/important findings and rerun full verification.
- [ ] Create PR against main; merge only after final-head CI is green.
- [ ] Record exact tested SHA, CI run, acceptance counts and remaining local WSL2 qualification in docs/STATUS.md.
