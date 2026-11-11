# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D5 are merged into `main`. The platform now includes a real Linux TCP HTTP server built from the existing parser, Router, static-file handler, response serializer, W6 bounded ThreadPool and W7 epoll/eventfd patterns.

**Latest merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932` (2026-11-11).  
**W8-D5 final-head CI:** Ubuntu 24.04 workflow run `42956321926`, all three jobs passed.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke). Final CI additionally passed root GCC/CTest 53/53, root ASan/UBSan 53/53, D5 fixed fixture under GCC and ASan/UBSan, reference workspace 10/10, Router fixture 43/43, clang-format and root/direct-fixture clang-tidy.

## W8-D5 implementation boundary

- Single-Reactor event loop owns listener/client sockets, epoll registrations, connection objects and read/write buffers.
- Worker tasks receive owned `HttpRequest` values and publish `HttpResponse` completions through a mutex-protected queue and eventfd wakeup.
- Generation tokens reject stale epoll events and worker completions after fd reuse.
- Bounded W6 ThreadPool submission is guarded by an event-loop-owned outstanding-job count; overload returns 503 without blocking the Reactor.
- Incremental request parsing, bounded request/response buffers, bounded static-file reads, partial writes and 400/413/500/503 handling are covered.
- The serializer still emits `Connection: close`; D5 intentionally serves one request per connection. Keep-Alive and pipelining are not implemented.

## Next stage: W8-D6

W8-D6 will evolve the one-request server into a controlled persistent HTTP/1.1 connection model. It must change serializer policy and connection lifecycle together rather than merely removing `Connection: close`.

Planned scope:
1. Parse and validate `Connection` tokens case-insensitively; implement HTTP/1.1 persistent default and explicit close, with HTTP/1.0 opt-in keep-alive only if fully specified.
2. Make response serialization accept an explicit connection policy and generate exactly one authoritative Connection header.
3. After a response is fully written, retain or close the connection according to request/response policy; process already-buffered subsequent requests without losing bytes.
4. Keep at most one in-flight request per connection initially; preserve ordered responses and avoid pipelined concurrent execution.
5. Add bounded request count per connection, idle/header/body deadlines, and limits for persistent connections.
6. Add graceful shutdown: stop accepting, stop dispatching new requests, finish bounded in-flight work until a deadline, then close remaining connections and join workers.
7. Add structured access/error logging with request metadata while avoiding unbounded body logging.
8. Extend fixed loopback tests for keep-alive reuse, explicit close, buffered second request, fragmented second request, request-count cap, idle timeout and shutdown under active work.

D6 will be split into independently testable slices: D6-A connection policy and serializer; D6-B sequential keep-alive lifecycle; D6-C deadlines and per-connection limits; D6-D graceful shutdown and structured logging. The exact protocol behavior and test counts will be frozen before implementation.

## Known boundaries

- Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain out of scope unless separately specified.
- Hosted Ubuntu CI does not replace target-machine Windows 11 + WSL2 acceptance.
- Passing automated checks proves tested behavior for the recorded source state, not human mastery or production readiness.
- No release tag is created until local WSL2 acceptance, reproducible build instructions and delivery checks are complete.

## Acceptance and contribution workflow

Use focused feature branches from current `main`. Record exact commands, compiler/tool versions, test counts, tested commit SHA and CI run. Merge only after final-head CI passes; keep local WSL2 acceptance separate and explicit.
