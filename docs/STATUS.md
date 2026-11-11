# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D6-B are merged into `main`. The platform includes a real Linux TCP HTTP server built from the parser, Router, static-file handler, response serializer, W6 bounded ThreadPool and W7 epoll/eventfd patterns.

**D5 functional merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932`.  
**D5 final-head CI:** Ubuntu 24.04 run `42956321926`, all three jobs passed.  
**D6-A merge:** `241059c60d3e0ea96c0b3658af1184acc539ab08` (PR #20).  
**D6-B merge:** `8597a38654dbdcebe2d406657d8862f89c0f2788` (PR #21).  
**D6-B final-head CI:** run `42971917343`, all three jobs passed.  
**D6-C branch:** `feat/w8-d6-connection-guards`; implementation and fixed tests are in progress.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke).
- W8-D6-A connection policy: 14 fixed tests against each of the two maintained protocol copies (28 executions). Final CI passed root GCC/CTest 53/53, root ASan/UBSan 53/53, HTTP Server reference workspace 10/10, D5 fixture 34/34 under sanitizer, D6-A fixture 28/28 under GCC and sanitizer, formatting and clang-tidy.
- W8-D6-B sequential Keep-Alive: eight fixed loopback tests for sequential reuse, pipelining, explicit close, fragmented subsequent requests, HTTP/1.0 defaults/opt-in, half-close draining and response ordering. Final-head run `42971917343` passed GCC, ASan/UBSan, clang-format and clang-tidy; root GCC and sanitizer each 53/53.

## W8-D6-C — connection deadlines and resource quotas (implementation in progress)

**Branch:** `feat/w8-d6-connection-guards`

- Added configurable idle (30 s), header (10 s) and body (15 s) deadlines, bounded to 24 hours.
- Added a timerfd-backed monotonic deadline queue with generation-based stale-entry rejection and periodic heap compaction.
- Idle expiry closes silently; incomplete headers/body expire with HTTP 408 and Connection: close.
- Header/body deadlines are absolute and are not extended by trickle traffic.
- Added a per-connection request cap (default 100); the final allowed response forces close.
- Added a persistent-connection quota (default 32), distinct from max_connections; when full, the current request is served with Connection: close, and slots are released on actual close.
- Added a nine-case fixed loopback fixture for timeout phases, absolute deadlines, idle refresh, request cap, persistent quota release and invalid configuration.
- GCC, ASan/UBSan and clang-tidy CI coverage has been added; final-head hosted results are pending.

## Remaining work

1. Complete D6-C final-head CI and review.
2. D6-D: graceful shutdown with bounded in-flight drain, structured access/error logs, runtime metrics and load/soak checks.
3. Windows 11 + WSL2 target acceptance, reproducible/offline dependency delivery and release qualification.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain outside the current scope.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
