# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D6-D2 are merged into `main`. The platform includes a real Linux TCP HTTP server with incremental parsing, Router/static-file handling, epoll Reactor, bounded W6 ThreadPool, sequential Keep-Alive, absolute connection deadlines, persistent-connection quotas, bounded graceful shutdown, structured JSONL logs and runtime metrics.

**D5 functional merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932`.  
**D5 final-head CI:** run `42956321926`, all three jobs passed.  
**D6-A merge:** `241059c60d3e0ea96c0b3658af1184acc539ab08` (PR #20).  
**D6-B merge:** `8597a38654dbdcebe2d406657d8862f89c0f2788` (PR #21).  
**D6-B final-head CI:** run `42971917343`, all three jobs passed.  
**D6-C merge:** `974b3d211412ebbdc54143d4a5a025a822ce3c85` (PR #22).  
**D6-C final-head CI:** run `42977367404`, all three jobs passed.  
**D6-D1 merge:** `d29d38ec04d4d5a0765f4585c9c25d9df69ab8c2` (PR #23).  
**D6-D1 final-head CI:** run `42979170395`, all three jobs passed.  
**D6-D2 merge:** `546f87c669d7cbf88429e87ed1bc7041313dfc63` (PR #24).  
**D6-D2 final-head CI:** run `42986255957`, all three jobs passed.  
**D6-D3 branch:** `feat/w8-d6-load-soak`; concurrent load, slow-reader/backpressure and repeated start/stop soak validation are in progress.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke).
- W8-D6-A connection policy: 14 fixed tests against each maintained protocol copy (28 executions).
- W8-D6-B sequential Keep-Alive: eight fixed loopback tests for sequential reuse, pipelining, explicit close, fragmented requests, HTTP/1.0, half-close draining and response ordering.
- W8-D6-C connection guards: nine fixed loopback tests for idle/header/body deadlines, absolute trickle-resistant deadlines, idle refresh, request-count cap, persistent-connection quota and invalid configuration. Final-head run `42977367404` passed root GCC/CTest 53/53, root ASan/UBSan 53/53, D6-C fixture 9/9 under GCC and sanitizer, D6-B fixture 8/8 under GCC and sanitizer, D6-A fixture 28/28 under GCC and sanitizer, formatting and clang-tidy across all direct fixtures.
- W8-D6-D1 graceful shutdown: five fixed loopback tests for idle connection close, in-flight response drain, incomplete-request 503, shutdown deadline and invalid configuration. Final-head run `42979170395` passed all three jobs; D6-D1 fixture 5/5 under GCC, ASan/UBSan and clang-tidy.
- W8-D6-D2 observability: eight fixed loopback tests for structured access logs, connection gauges, handler failures, log channel controls, query redaction, sink failure containment, timeout counters, connection rejections and worker queue pressure. Final-head run `42986255957` passed all three jobs; root GCC/CTest 53/53, root ASan/UBSan 53/53, D6-D2 fixture 8/8 under GCC, ASan/UBSan and clang-tidy.

## W8-D6-D3 — load, backpressure and soak validation (implementation in progress)

**Branch:** `feat/w8-d6-load-soak`

- Concurrent Keep-Alive clients with request/response accounting and zero queue rejection under a bounded test workload.
- Slow-reader test with a multi-megabyte response to force non-blocking send backpressure and verify the response is resumed without truncation.
- Repeated server construction/start/stop cycles with ephemeral ports and connection/job gauges returning to zero.
- Bounded soak loop with correctness checks and no active connection/job leaks after each run.
- Add fixed loopback fixture and GCC, ASan/UBSan and clang-tidy CI gates; final-head hosted results are pending.

## Remaining work

1. Complete D6-D3 final-head CI and review.
2. Windows 11 + WSL2 target acceptance, reproducible/offline dependency delivery and release qualification.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain outside the current scope.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
