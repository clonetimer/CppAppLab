# W8-D6-C · HTTP Connection Deadlines and Resource Quotas

## Goal

Bound the lifetime and reuse of accepted TCP connections. Keep-Alive must not turn an idle socket, a slow partial request, or an unlimited request sequence into an unbounded resource reservation.

## Configuration

`HttpServerConfig` adds:

| Setting | Default | Meaning |
|---|---:|---|
| `idle_timeout` | 30 s | Maximum time a newly accepted or idle persistent connection may wait for the next request byte |
| `header_timeout` | 10 s | Absolute time allowed to complete the request line and headers after the first request byte |
| `body_timeout` | 15 s | Absolute time allowed to complete a request body after the complete header block arrives |
| `max_requests_per_connection` | 100 | Maximum requests dispatched on one TCP connection |
| `max_persistent_connections` | 32 | Maximum connections concurrently granted Keep-Alive permission |

Timeouts must be positive and no greater than 24 hours. Request and persistent-connection limits must be positive. Existing `max_connections`, `max_request_bytes`, `max_response_bytes` and worker-queue limits remain in force.

## Deadline semantics

- An accepted socket starts with an idle deadline.
- The first request byte switches it to a header deadline.
- Once CRLFCRLF is present but the body is incomplete, it switches to a body deadline.
- After a complete request is dispatched, its receive deadline is cancelled while the handler and response are in flight.
- After a Keep-Alive response is fully written, the next buffered request is dispatched; otherwise the connection receives a fresh idle deadline.
- Header and body deadlines are absolute, not inactivity timers. Sending one byte at a time does not extend them.
- Idle timeout closes silently because no request is pending.
- Header/body timeout returns HTTP 408 Request Timeout with Connection: close.
- A peer half-close still follows D6-B: complete buffered requests are drained; an incomplete request at EOF receives 400 rather than waiting for a timeout.

The implementation uses one Linux timerfd and an event-loop-owned min-heap of deadline entries. Connection generation values invalidate stale heap entries after a deadline changes or a connection closes. Heap compaction bounds stale-entry memory growth during repeated Keep-Alive use.

## Request and persistent-connection quotas

- The request counter increments when a complete request is accepted for dispatch.
- The response for the configured final request carries Connection: close; any later pipelined bytes are discarded.
- A persistent slot is reserved only when a request would otherwise receive Keep-Alive.
- If all persistent slots are occupied, the current request is still served, but its response is downgraded to Connection: close. Close-only requests remain serviceable.
- A reserved slot is released when the TCP connection is actually closed, including idle timeout, request timeout, peer disconnect, and explicit close.

The quota is deliberately separate from `max_connections`: short-lived close-only clients may still be served while the persistent-connection budget is full.

## Acceptance

The fixed loopback fixture covers:

1. Silent close of an idle connection.
2. HTTP 408 for incomplete headers.
3. HTTP 408 for an incomplete request body.
4. Absolute header deadline is not extended by trickle bytes.
5. Absolute body deadline is not extended by trickle bytes.
6. Idle deadline is refreshed after a completed Keep-Alive response.
7. Maximum requests per connection forces close after the final permitted response.
8. Persistent-connection quota downgrades excess Keep-Alive requests and releases slots on close.
9. Invalid zero timeout and quota settings are rejected.

The fixture runs independently under GCC, ASan/UBSan and clang-tidy. D6-B's eight Keep-Alive lifecycle tests and D5's one-request regression fixture remain separate gates.

## Explain your design

1. Why are idle, header and body timeouts separate phases?
2. Why must header/body deadlines be absolute rather than reset after each byte?
3. Why does an idle timeout close silently while an incomplete request receives 408?
4. Why is the persistent-connection quota distinct from the total accepted-connection limit?
5. Why is the persistent slot released on actual socket close rather than as soon as a request asks for Connection: close?
6. Why does the final allowed request carry Connection: close before the server discards any later pipelined bytes?
