# W8-D5 HTTP TCP Server — Design

## Intent

Turn the already merged W8-D1–D4 application pipeline into a real Linux HTTP server. The listener accepts TCP clients, parses requests incrementally, dispatches Router/static-file work through the existing W6 thread pool, and returns serialized HTTP responses over non-blocking sockets.

## Goals

- Use a non-blocking IPv4 listener and level-triggered epoll.
- Keep socket I/O, epoll registration, connection maps, buffers and descriptor lifetime exclusively on the event-loop thread.
- Submit owned HttpRequest values to the W6 ThreadPool; workers return HttpResponse values through a completion queue.
- Wake the event loop with eventfd after a worker posts a completion.
- Protect against stale worker completions and epoll events with monotonically increasing connection-generation tokens, not fd alone.
- Bound active connections, request buffering, response buffering and worker queue occupancy.
- Convert parser errors to 400, oversized requests to 413, worker/handler failures to 500, and overload to 503.
- Preserve existing Router, parser, static-file handler and serializer behavior.

## Non-goals

- HTTP keep-alive, TLS, chunked transfer, URL decoding, query-parameter parsing, route patterns, production-grade filesystem canonical/symlink containment, and release qualification. These are later stages.
- No worker may call recv/send/close/epoll_ctl or mutate a Connection.

## Approaches

1. Single-threaded epoll only: simplest, but a slow handler or file read blocks every client. Rejected because D5 explicitly teaches moving business work off the event loop.
2. Thread per request/connection: simple to demonstrate but unbounded thread creation and difficult socket lifetime ownership. Rejected.
3. Single Reactor + bounded W6 ThreadPool + completion queue/eventfd: recommended and selected. The event loop exclusively owns sockets and connections; workers receive request values and return response values. This follows the supplied D5 source and reuses W6.

## Implementation decisions

- Use W6 ThreadPool unchanged. Its bounded submission is blocking, so maintain an event-loop-owned outstanding_jobs counter that includes queued and currently running tasks. Before submit, return 503 if outstanding_jobs == task_queue_capacity; increment before submit and decrement for every worker completion, including stale completions. This guarantees the event loop never waits for queue capacity even when clients disconnect while workers are still running. max_connections remains an independent socket/resource limit.
- Use a monotonic uint64_t generation token in epoll_event.data.u64 and in every completion. A completion is applied only if both fd and generation still identify the same live connection.
- Use a mutex-protected completion deque and nonblocking eventfd. Each connection has at most one in-flight request; outstanding_jobs bounds queued, running and not-yet-drained completions by task_queue_capacity.
- Keep the existing D3 serializer Connection: close contract. D5 therefore serves one request per TCP connection and closes after the response is fully sent. The supplied D5 discussion of parsing a second pipelined request after completion becomes active with D6 keep-alive; it cannot be enabled without changing the D3 serializer contract. This is an explicit compatibility ruling, not a silent behavior change.
- Use level-triggered epoll and drain recv/send until EAGAIN/EWOULDBLOCK. Enable EPOLLOUT only while the write buffer is non-empty.
- The server constructor binds/listens and exposes the actual bound port (including ephemeral port 0). run() blocks on the caller thread; request_stop() wakes it via eventfd. The caller must join the run thread before destroying the server.
- Static-file responses exceeding the configured response limit are replaced with a bounded 500 response; malformed requests receive 400, parser/body limit failures receive 413, and rejected connections/worker queue saturation and connection-limit rejection receive 503.

## Acceptance

The fixed fixture must exercise actual loopback TCP sockets and the real parser → dispatcher → serializer pipeline. It must cover dynamic GET, binary POST echo, health, static fallback, malformed/oversized input, partial writes, connection limits, bounded worker saturation, handler exceptions, stale completion rejection and eventfd shutdown. Root acceptance must use the platform-owned fixture and ignore learner-controlled CMake/tests.
