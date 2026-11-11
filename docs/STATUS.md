# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D6-A are merged into `main`. The platform includes a real Linux TCP HTTP server built from the parser, Router, static-file handler, response serializer, W6 bounded ThreadPool and W7 epoll/eventfd patterns.

**D5 functional merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932`.  
**D5 final-head CI:** Ubuntu 24.04 run `42956321926`, all three jobs passed.  
**D6-A merge:** `241059c60d3e0ea96c0b3658af1184acc539ab08` (PR #20).  
**D6-A final-head CI:** run `42968748767`, all three jobs passed.  
**D6-B tested head:** `321a708476c3d170af9b8e5854df975548b73268`.  
**D6-B final-head CI:** Ubuntu 24.04 run `42971176071`, all three jobs passed; PR #21 is awaiting merge.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke).
- W8-D6-A connection policy: 14 fixed tests against each of the two maintained protocol copies (28 executions). Final CI passed root GCC/CTest 53/53, root ASan/UBSan 53/53, HTTP Server reference workspace 10/10, D5 fixture 34/34 under sanitizer, D6-A fixture 28/28 under GCC and sanitizer, formatting and clang-tidy.

## W8-D6-B — sequential Keep-Alive lifecycle (implemented; awaiting merge)

**Branch:** `feat/w8-d6-keep-alive-lifecycle`  
**PR:** #21  
**Tested head:** `321a708476c3d170af9b8e5854df975548b73268`  
**CI:** run `42971176071`, all three jobs passed.

- The server stores the policy and request protocol version before handing the request to a worker.
- The serializer emits HTTP/1.0 or HTTP/1.1 status lines to match the request.
- A response must be fully written before the next request is dispatched.
- EPOLLIN is disabled while a request is processing or a response is being written; EPOLLOUT is used only while bytes remain.
- The parser's consumed count removes one request prefix and preserves pipelined bytes.
- Explicit close stops after the current response; complete buffered requests are drained after a peer half-close, then the server closes.
- The D5 fixed fixture and CLI smoke send Connection: close explicitly, retaining their original one-request regression contract.
- The D6-B loopback fixture covers eight cases: sequential reuse, pipelining, explicit close, fragmented subsequent requests, HTTP/1.0 default close, HTTP/1.0 keep-alive opt-in, half-close pipeline draining, and strict handler/response ordering.
- Final-head GCC: root 53/53, HTTP Server reference workspace 10/10, D6-A fixture 28/28, D6-B fixture 8/8.
- Final-head ASan/UBSan: root 53/53, Router fixture 43/43, D5 HTTP Server fixture 34/34, D6-A fixture 28/28, D6-B fixture 8/8.
- Final-head clang-format and clang-tidy passed, including direct Router, HTTP Server, connection-policy and Keep-Alive fixtures.

D6-B does not yet add timeouts, request-count caps or graceful shutdown. Those remain D6-C/D6-D.

## Remaining D6 work

1. Merge D6-B after final-head CI and review — CI is green; PR #21 is awaiting merge.
2. D6-C: idle/header/body deadlines, per-connection request count and persistent connection limits.
3. D6-D: graceful shutdown, structured access/error logging, metrics and load/soak checks.
4. Windows 11 + WSL2 target acceptance, reproducible/offline dependency delivery and release qualification.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain outside the current scope.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
