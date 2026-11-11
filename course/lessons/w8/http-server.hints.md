## Hint 1 — Reuse the existing modules

Do not rewrite the HTTP parser, Router, static-file handler, serializer or W6 ThreadPool. Compose the W8-D4 dispatcher with the W6 pool and W7 socket/epoll patterns.

## Hint 2 — Keep ownership on one thread

The event loop owns all file descriptors, epoll registrations, connection maps and buffers. A worker receives an HttpRequest value and returns an HttpResponse value. Never capture a Connection reference or socket-owning pointer in a worker task.

## Hint 3 — Use a generation token

Store a monotonically increasing uint64 token in each Connection and epoll_event.data.u64. Put both fd and token in a completion. Discard a completion unless the token still maps to that same fd.

## Hint 4 — Wake the event loop

Push the completion under a mutex, then write uint64_t{1} to a non-blocking eventfd. The event loop drains eventfd and then drains the completion queue. EAGAIN on eventfd write means a wake is already pending.

## Hint 5 — Avoid blocking ThreadPool submission

The existing W6 submit waits for bounded queue capacity. Allow only one in-flight request per connection and maintain outstanding_jobs across queued, running and undrained completions. Before calling submit, return 503 when outstanding_jobs reaches task_queue_capacity; decrement only when the matching completion is drained, even if its connection is stale.

## Hint 6 — Incremental receive and bounded buffers

Use recv until EAGAIN/EWOULDBLOCK. Append bytes to readBuffer, run HttpRequestParser and distinguish NeedMoreData, Complete and Error. Map parser/header/body size failures to 413 and malformed input to 400.

## Hint 7 — Partial writes

Serialize the HttpResponse once into writeBuffer. Enable EPOLLOUT only while bytes remain. Advance writeOffset after each successful send; retry EINTR; stop on EAGAIN. Close only after the response is fully sent.

## Hint 8 — Preserve D5's protocol boundary

The existing response serializer always emits Connection: close. Therefore D5 serves one request per TCP connection. Keep-Alive, pipelining and re-parsing buffered requests belong to the next task after the serializer and connection policy are changed together.

## Hint 9 — Error and shutdown paths

Catch handler exceptions in the worker and return 500. Return 503 when capacity is exhausted. request_stop sets the stop flag and writes eventfd; the caller joins run() before destroying HttpServer.


## Hint 10 — Concurrent handlers

Router callbacks run on multiple W6 worker threads. Keep handlers stateless where possible. If a callback captures shared mutable state, protect it with a mutex or an appropriate atomic; never capture a Connection or file descriptor.
