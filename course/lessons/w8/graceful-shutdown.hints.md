## Hint 1 — Wake the owner

request_stop should set an atomic stop flag and wake the Reactor through eventfd. The Reactor, not the caller or a worker, owns listener and client descriptors.

## Hint 2 — Separate idle, incomplete and in-flight connections

Close idle persistent connections silently. Return 503 for incomplete requests. Let in-flight handlers finish, but force their response policy to Connection: close. Finish a partially written response when possible.

## Hint 3 — Bound socket drain, not arbitrary C++ execution

Track a CLOCK_MONOTONIC shutdown deadline in the timerfd scheduler. On expiry, close remaining sockets, but continue draining worker completions. The ThreadPool cannot forcibly cancel an arbitrary handler; run() still waits for submitted callbacks to return.
