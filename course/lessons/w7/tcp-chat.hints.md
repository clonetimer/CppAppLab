## Hint 1 — Event loop ownership

Let one thread own the epoll descriptor, listener, client map and per-client buffers. Other threads should request shutdown through an eventfd rather than mutating client state.

## Hint 2 — Non-blocking accept

Use `accept4(listener, ..., SOCK_NONBLOCK | SOCK_CLOEXEC)` in a loop. Stop when it returns `EAGAIN` or `EWOULDBLOCK`; retry `EINTR`.

## Hint 3 — Input framing

Append received bytes to a per-client input buffer. Extract every complete newline-terminated line. Retain an incomplete suffix for the next EPOLLIN event. Reject a line or buffer that exceeds the configured limit.

## Hint 4 — Output buffering

Append broadcasts to each recipient's output buffer. Attempt send immediately; if bytes remain, enable EPOLLOUT. On EPOLLOUT, continue from the saved offset until the buffer is empty or send returns EAGAIN. Disable EPOLLOUT when no bytes remain.

## Hint 5 — Backpressure

Bound each client's queued output. Disconnect or shed a slow client when its pending output exceeds the configured maximum; never allow an unbounded broadcast queue.

## Hint 6 — Stop wakeup

Create an eventfd, register it with epoll, and write to it from `request_stop()`. The event loop reads the counter and exits through one cleanup path.
