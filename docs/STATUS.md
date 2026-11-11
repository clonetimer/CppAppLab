# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D5 are merged into `main`. The platform includes a real Linux TCP HTTP server built from the parser, Router, static-file handler, response serializer, W6 bounded ThreadPool and W7 epoll/eventfd patterns.

**Latest functional merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932` (2026-11-11).  
**D5 final-head CI:** Ubuntu 24.04 run `42956321926`, all three jobs passed.  
**Documentation reconciliation:** PR #19 merged as `cbb8a5d48c4cf42517b01cc2c2b216452ac7a435`.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke). Final CI passed root GCC/CTest 53/53, root ASan/UBSan 53/53, D5 fixture under GCC and ASan/UBSan, reference workspace 10/10, Router fixture 43/43, clang-format and root/direct-fixture clang-tidy.

## W8-D6-A — HTTP connection policy (implementation in progress)

**Branch:** `feat/w8-d6-connection-policy`  
**Status:** implementation and dedicated fixed fixture added; hosted CI is pending.

- Added `ConnectionPolicy::{Close, KeepAlive}`.
- Added request policy resolution: HTTP/1.1 persistent by default; HTTP/1.0 close by default with explicit keep-alive opt-in; close takes precedence.
- Connection option matching is case-insensitive, comma-token based, trims optional whitespace and does not match substrings.
- Added an explicit serializer overload that emits exactly one authoritative Connection header and computes Content-Length.
- Kept the legacy one-argument serializer close-only so D1–D5 behavior remains unchanged until D6-B integrates socket reuse.
- Added the same 14-case acceptance suite for both the original HTTP protocol reference and the Router protocol copy used by the server (28 executions total when both targets pass).
- Added CI coverage for GCC, ASan/UBSan and clang-tidy on both copies.

D6-A does not yet enable persistent sockets. The D5 server continues to close after each response.

## Remaining D6 work

1. Complete D6-A final-head CI and code review.
2. D6-B: integrate request connection policy into the Reactor; retain eligible connections after fully written responses; process buffered and fragmented subsequent requests sequentially; preserve response order.
3. D6-C: idle/header/body deadlines, per-connection request count and persistent connection limits.
4. D6-D: graceful shutdown, structured access/error logging, metrics and load/soak checks.
5. Windows 11 + WSL2 target acceptance, reproducible/offline dependency delivery and release qualification.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain outside the current scope.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
