## Hint 1 — Emit events from the owner thread

Create JSONL access, error and lifecycle events. Keep the default sink on stderr and allow a custom callback. A callback runs on the Reactor thread, so it must not block and must copy string_view data it retains.

## Hint 2 — Do not log secrets

Record method, path, status, duration, response bytes and connection policy. Redact the query string; do not log request headers, request bodies or raw exception messages.

## Hint 3 — Separate counters from gauges

Use atomics for the metrics snapshot. Counters accumulate; active connections, persistent connections and active jobs are gauges. Count a response only after all serialized bytes have been written. A snapshot is thread-safe per field, not transactionally consistent across fields.
