## Hint 1 — Model the request phases

An accepted idle socket starts with an idle deadline. The first request byte starts the header deadline. A complete header block with an incomplete body switches to the body deadline. Cancel receive deadlines while the handler/response is in flight.

## Hint 2 — Use one monotonic timer

Use timerfd with CLOCK_MONOTONIC and an event-loop-owned min-heap. Store a generation on each connection so refreshed or closed connections invalidate old heap entries. Header/body deadlines are absolute; trickle bytes must not reset them.

## Hint 3 — Release quotas at the resource boundary

Reserve a persistent slot only when granting Keep-Alive. If full, serve the current request with Connection: close. The configured final request also forces close. Release the slot when the socket is actually closed, not when the response is merely queued.
