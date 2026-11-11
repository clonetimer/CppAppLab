# W8-D5 · epoll + ThreadPool HTTP Server

## Goal

Turn the W8-D1–D4 HTTP application pipeline into a real Linux TCP server. This stage combines three capabilities already studied:

- W6: bounded ThreadPool and futures
- W7: non-blocking sockets, epoll and eventfd
- W8-D1–D4: HTTP parser, Router, static-file handler and response serializer

The deliverable is a single-Reactor HTTP server. It listens on a real TCP port and serves the existing dynamic and static routes.

## Core ownership rule

**The event-loop thread exclusively owns sockets, epoll registrations, Connection objects and read/write buffers. Worker threads never call recv, send, close or epoll_ctl, and never retain a Connection pointer/reference.**

The event loop transfers an owned HttpRequest value to a worker. The worker returns an HttpResponse value through a completion queue. eventfd wakes the event loop so it can append serialized bytes to the connection's write buffer.

## Architecture

```text
TCP clients
    |
    v
non-blocking listener / epoll
    |
    +-- accept / recv / send / close
    |
    v
incremental HttpRequestParser
    |
    v
bounded W6 ThreadPool
    |
    +-- Router / dynamic handlers / static file I/O
    |
    v
HttpResponse
    |
    v
completion queue + eventfd
    |
    v
event-loop thread
    |
    v
serialize_response -> writeBuffer -> EPOLLOUT -> send
```

## Connection and stale-result safety

A file descriptor is not a stable connection identity because Linux can reuse its integer value immediately after close. Every accepted connection therefore receives a monotonically increasing 64-bit generation token.

- epoll events carry the generation token in data.u64.
- Worker completions carry both fd and generation.
- A completion is applied only if the generation still exists and the fd matches.
- A stale completion for a disconnected client is discarded.

The event loop is the only thread that mutates the connection maps. Workers own request/response values only.

## Bounded concurrency and overload

The existing W6 ThreadPool uses a blocking bounded submission queue. Connection count alone is not a sufficient bound: a disconnected client may leave a worker running while a new client connects.

The event loop therefore maintains an `outstanding_jobs` counter covering queued jobs, running jobs and completions not yet drained. It checks the counter before submission, returns 503 at `task_queue_capacity`, increments before submit and decrements for every drained completion, including stale completions. Thus the event loop never blocks waiting for queue space. `max_connections` remains an independent socket/resource limit.

When the active connection limit is reached, newly accepted clients receive a best-effort 503 Service Unavailable response and are closed. When the worker outstanding-job limit is reached, the request receives 503. Request and response buffers are also bounded.

## D5 protocol boundary

D3's response serializer deliberately emits `Connection: close`. D5 preserves that contract and serves one request per TCP connection, closing only after the full response has been sent. Keep-Alive and HTTP pipelining are D6 work; they must not be silently enabled while the serializer still forces close.

Chunked Transfer-Encoding, TLS, URL decoding, query parameter parsing, route patterns and production-grade canonical/symlink containment remain out of scope.

## Error mapping

- malformed/incomplete HTTP request: 400 Bad Request
- request/header/body over configured or parser limit: 413 Content Too Large
- handler exception: 500 Internal Server Error
- active connection limit or worker submission failure: 503 Service Unavailable
- unknown route/static file: existing 404 behavior
- known dynamic route with wrong method: existing 405 + Allow behavior

A response larger than the configured response limit is replaced with a bounded 500 response.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/http-server server 8080
```

Try the actual network server:

```bash
curl -i http://127.0.0.1:8080/hello
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
```

Use Ctrl+C to stop the demonstration process. The library API also exposes thread-safe `request_stop()`, which writes eventfd and wakes a blocked epoll_wait. The caller must join the thread running `run()` before destroying the server.

## What the platform checks

The fixed fixture uses real loopback TCP sockets and the real parser → dispatcher → serializer path. It covers dynamic and static routes, binary Echo, fragmented requests, partial writes, malformed/oversized input, overload, handler exceptions, stale worker completion and eventfd shutdown. Platform-owned CMake and tests remain authoritative; learner-controlled build files cannot weaken the acceptance check.

## Acceptance

- [ ] Real non-blocking IPv4 listener and level-triggered epoll.
- [ ] Event-loop-only ownership of sockets and Connection state.
- [ ] Bounded W6 ThreadPool integration with owned request/response values.
- [ ] Completion queue and eventfd wakeup.
- [ ] Generation-safe stale event/completion rejection.
- [ ] Incremental parser and bounded read/write buffers.
- [ ] Partial writes resumed through EPOLLOUT only when needed.
- [ ] 400 / 413 / 500 / 503 mapping and clean shutdown.
- [ ] All 30 fixed acceptance checks pass.

## Explain your design

1. Why does epoll alone not prevent a slow handler from blocking other clients?
2. Why must workers not modify Connection or socket state?
3. Why is fd alone insufficient as a connection identity?
4. What does the generation token protect against?
5. Why does eventfd wake the event loop after a completion is queued?
6. Why is the completion queue bounded by outstanding worker jobs, including jobs whose clients disconnected?
7. Why must the event loop check outstanding_jobs before calling the blocking W6 submit?
8. Why is there only one in-flight request per connection in D5?
9. Why does D5 close after one response, and what must change before Keep-Alive?
10. Why should EPOLLOUT be enabled only while writeBuffer is non-empty?
11. What happens when a client disconnects while a worker is still processing?
12. Why must the event loop discard completions whose generation no longer exists?
