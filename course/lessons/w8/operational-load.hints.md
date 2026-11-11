## Hint 1 — Bound the workload before starting threads

Validate IPv4 host, port, request path, client count, requests per client and timeout. Each worker owns one socket and performs its requests sequentially; the final request asks the server to close the connection.

## Hint 2 — Use non-blocking socket operations with deadlines

Connect with a non-blocking socket and poll for completion. Handle EINTR and EAGAIN for send/recv. Parse the response status line and Content-Length so a partial response is not counted as success.

## Hint 3 — Aggregate measurements after joining workers

Count every attempted request as either succeeded or failed. Record per-request steady-clock latency, sort the samples, and calculate p50/p95/p99. Compute throughput from successful requests divided by total elapsed time; do not invent a machine-independent performance threshold.
