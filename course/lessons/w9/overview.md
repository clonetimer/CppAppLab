# W9–W12 · Advanced C++ Network Services Roadmap

## Direction

W1–W8 establish C++17, Linux system programming, concurrency, TCP and a working HTTP server. W9–W12 turn that educational server into a security-conscious, measurable and operationally complete network service.

The advanced curriculum remains cumulative: learners extend the W8 server and its protocol components rather than replacing them with unrelated toy examples. Every registered task must have an isolated learner workspace, a focused source boundary, progressive hints, platform-owned tests and a reproducible reference implementation.

## W9 · HTTP protocol completeness and security

**W9-A1 merged into main:** PR #27, commit bfb3f3f; GCC, ASan/UBSan and clang-tidy acceptance passed.

| Stage | Planned task | Engineering outcome |
|---|---|---|
| A1 | w9.http-chunked | Incremental chunked request-body decoding, trailers, exact consumed-byte accounting and unambiguous message framing |
| A2 | w9.request-target | Origin-form target parsing, strict percent decoding, query multimap and malformed-escape rejection |
| A3 | w9.static-root | Document-root confinement using directory file descriptors, openat, no-follow semantics and regular-file checks |
| A4 | w9.http-hardening | Request-smuggling regression cases, header/framing policy, resource limits and deterministic protocol errors |
| A5 | w9.http-fuzzing | Parser fuzz target, regression corpus, sanitizer-backed fuzz smoke and minimized reproductions |

A1 (chunked transfer decoding) is merged into main. A2 (request-target and query parsing) is being implemented on feat/w9-request-target-query with a learner workspace and 39-case fixture. A3–A5 remain roadmap stages until their learner source boundaries and fixed fixtures are implemented and validated.

### W9-A1 contract

- Accept exactly one transfer coding: chunked, for HTTP/1.1 requests.
- Reject simultaneous Content-Length and Transfer-Encoding.
- Reject unsupported coding chains rather than silently interpreting them.
- Decode arbitrary binary chunks incrementally.
- Bound decoded body size, chunk-size line length and trailer section size.
- Validate trailer fields and reject framing/routing fields.
- Preserve bytes after the completed request for the next pipelined parse.

The first learner fixture is designed to compile the learner's parser source, not the reference source. Its 35 platform-owned tests cover complete and partial messages, malformed input, limits, trailers, pipelining and Content-Length regression.

## W10 · TLS and WebSocket services

Planned outcomes:

1. OpenSSL ownership, certificate/key loading, TLS context configuration and deterministic cleanup.
2. Non-blocking TLS handshake integrated with the existing epoll Reactor.
3. TLS read/write retry semantics, WANT_READ/WANT_WRITE transitions, deadlines and shutdown.
4. HTTP Upgrade handshake for WebSocket.
5. WebSocket frame parser: masking, extended lengths, fragmentation, control frames, ping/pong and close.
6. A loopback integration task that runs a TLS WebSocket echo service and verifies lifecycle/error behavior.

TLS is not a thin socket wrapper: SSL state must remain owned by the Reactor connection and must not be mutated concurrently by unrelated workers.

## W11 · High-performance C++ service engineering

Planned outcomes:

- profiling before optimization; repeatable workloads and baseline reports
- std::pmr and allocator-aware request/response buffers
- buffer reuse, scatter/gather I/O and sendfile for static-file responses
- batching, bounded queues and backpressure under overload
- cache locality, false sharing and atomic contention analysis
- throughput and p50/p95/p99 latency measurements with environment metadata
- performance changes accepted only when correctness and sanitizer regressions remain green

No fixed throughput target is assumed across different machines. Performance tasks compare measured before/after results under the same declared workload.

## W12 · Production service operations and reliability

Planned outcomes:

- typed configuration, validation, defaults and reload policy
- signal handling through the Reactor wakeup path
- readiness/liveness endpoints and dependency-independent health
- structured logs, correlation IDs and trace propagation
- systemd unit, least-privilege runtime user, working directory and restart policy
- bounded shutdown, rolling restart and failure recovery
- install/package scripts, offline dependencies, release artifacts and operational runbook

## Completion criteria

A stage is not considered complete because a lesson exists. It is complete only when:

1. the learner implementation is compiled by a platform-owned fixture;
2. boundary, negative, resource and lifecycle tests exist;
3. the reference implementation passes GCC, ASan/UBSan and clang-tidy;
4. the task is exercised through the real cppapplab start/lesson/hint/check workflow;
5. the curriculum, task catalog, CI and status evidence agree;
6. platform-specific acceptance is reported separately from hosted CI.
