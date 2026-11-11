# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D6-D3 are merged into `main`. The platform includes a real Linux TCP HTTP server with incremental parsing, Router/static-file handling, epoll Reactor, bounded W6 ThreadPool, sequential Keep-Alive, absolute connection deadlines, persistent-connection quotas, bounded graceful shutdown, structured JSONL logs, thread-safe runtime metrics, and fixed operational load/soak regression coverage.

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
**D6-D3 merge:** `597a8a00a3052b70bf1af7f695c63f7310d67563` (PR #25).  
**D6-D3 final-head:** `736a0bac1e859c8cdffbfe7b1883e709cb928eec`; CI run `42991542424`, all three jobs passed.  
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
- W8-D6-D2 observability: eight fixed loopback tests for structured access logs, query redaction, connection gauges, handler failures, log-sink exception isolation, timeout counters, connection rejection and worker-queue rejection. Final-head run `42986255957` passed all three jobs; root GCC/CTest 53/53, root ASan/UBSan 53/53, HTTP Server reference workspace 10/10, D6-A 28/28, D6-B 8/8, D6-C 9/9, D6-D1 5/5 and D6-D2 8/8. Formatting and clang-tidy passed for all configured direct fixtures.

## W8-D6-D3 — operational load and soak validation (merged)

**Merge:** PR #25, commit `597a8a00a3052b70bf1af7f695c63f7310d67563`.  
**Final tested head:** `736a0bac1e859c8cdffbfe7b1883e709cb928eec`.  
**Final CI:** run `42991542424`, all three jobs passed.

The four-case loopback fixture in `course/fixtures/w8/http-operational` verifies:

1. Sixteen concurrent clients; handler concurrency remains within the four-worker bound and all responses complete.
2. An 8 MiB response to a deliberately slow reader; non-blocking backpressure, EPOLLOUT recovery and complete body delivery.
3. Eight concurrent Keep-Alive clients × forty requests (320 total); no queue rejection, protocol errors or leaked connections/jobs.
4. Eight repeated server construction/run/stop/join/destruction cycles with idempotent stop requests.

Final-head regression evidence: root GCC/CTest 53/53; root ASan/UBSan 53/53; HTTP Server reference workspace 10/10; D6-A 28/28; D6-B 8/8; D6-C 9/9; D6-D1 5/5; D6-D2 8/8; and D6-D3 4/4 under GCC, ASan/UBSan and clang-tidy. Formatting and clang-tidy passed across the configured direct fixtures.

These are deterministic functional load/soak regressions and resource-recovery checks, not throughput, tail-latency or production-capacity SLAs.

## Course catalog boundary

The formal learning catalog currently contains 14 tasks through `w8.http-server` (D5). D6-A through D6-D3 are integrated into the reference server and have dedicated lessons and acceptance fixtures, but are not yet independent learner task IDs with their own workspaces, hints and `cppapplab check` contracts.

## Remaining work

1. Convert the D6 engineering extensions into staged learner-facing tasks with focused source boundaries, hints and fixed platform-owned acceptance.
2. Complete Windows 11 + WSL2 target acceptance.
3. Finish reproducible/offline dependency delivery, first-use guidance and release qualification.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain outside the current scope.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
