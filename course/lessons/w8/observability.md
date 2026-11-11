# W8-D6-D2 · Structured Logs and Runtime Metrics

## Goal

Make the HTTP server observable without allowing worker threads to mutate Reactor-owned state or exposing request secrets in routine logs.

## Structured JSON Lines

By default, the server writes one JSON object per line to stderr. Access and error channels can be independently disabled:

- `access_log_enabled`: successful and error HTTP response completion records.
- `error_log_enabled`: handler failures, parser/HTTP errors, connection rejections, timeouts, and server lifecycle events.
- `log_sink`: optional callback that receives a temporary `std::string_view` on the Reactor thread. A sink must copy the view if it retains the line and must not block the Reactor.

The default sink is `std::clog`. Callback exceptions are contained and counted; they do not terminate request processing.

### Access event

An `http_access` event contains:

- wall-clock `timestamp_ms`, severity `level`, and event name
- connection generation ID
- request method and path
- HTTP status
- end-to-end request-to-complete-write duration in milliseconds
- fully written response byte count
- whether the connection will close or remain persistent

The query component is intentionally redacted. The log records `query_present` instead. Request headers and bodies are never logged.

### Error and lifecycle events

The event stream includes `http_error`, `handler_failure`, `worker_queue_rejected`, `connection_rejected`, `idle_timeout`, `header_timeout`, `body_timeout`, `server_started`, `server_shutdown_started`, `shutdown_deadline_expired`, `server_loop_failure`, and `server_stopped`.

Handler exceptions are mapped to a fixed diagnostic event. Raw exception text is not emitted because handlers may include credentials or request content in exception messages.

## Runtime metrics API

`HttpServer::metrics()` returns a thread-safe `HttpServerMetrics` snapshot. Counters are backed by atomics, so the API can be called while the Reactor and worker pool are running.

| Metric | Semantics |
|---|---|
| `accepted_connections` | Sockets successfully registered with epoll |
| `rejected_connections` | New sockets refused by `max_connections` |
| `active_connections` | Current accepted sockets owned by the Reactor |
| `closed_connections` | Accepted sockets retired by the Reactor |
| `persistent_connections` | Current sockets holding a Keep-Alive quota slot |
| `requests_received` | Complete HTTP requests accepted for dispatch |
| `responses_completed` | Responses fully written to their peer |
| `response_bytes` | Total bytes in fully written HTTP responses |
| `client_errors` | Fully written 4xx responses |
| `server_errors` | Fully written 5xx responses |
| `request_timeouts` | Header/body deadline expirations |
| `idle_timeouts` | Idle keep-alive/accepted socket expirations |
| `handler_failures` | Exceptions thrown by route/static handlers |
| `queue_rejections` | Requests rejected because the worker queue is full or submission fails |
| `active_jobs` | Current submitted worker jobs not yet completed |
| `graceful_shutdowns` | Shutdown sequences started |
| `forced_shutdown_connections` | Sockets closed when the shutdown drain deadline expires |
| `log_sink_failures` | User log callbacks that threw |

Metrics are cumulative except gauges (`active_connections`, `persistent_connections`, `active_jobs`). Response counters advance only after the entire serialized response is written; a peer disconnect during a partial write does not count as a completed response. Rejected connections are not accepted sockets and therefore do not increment `closed_connections`.

The snapshot is thread-safe per field but is not a transactional cross-counter snapshot: values may advance between individual atomic loads.

## Acceptance

The fixed loopback fixture covers:

1. JSONL access record fields and completed response counters.
2. Active/persistent connection gauges across two requests on one Keep-Alive socket.
3. Handler exception event, 500 counter and exclusion of raw exception text.
4. Independent error-log disabling and query-string redaction.
5. Log-sink exceptions are contained and counted without interrupting request processing.
6. Idle and incomplete-header timeout counters.
7. Maximum-connection rejection counter.
8. Worker-queue rejection counter and active-job recovery.

The fixture runs under GCC, ASan/UBSan and clang-tidy. D5, D6-B, D6-C and D6-D1 fixed fixtures remain independent regression gates.

## Explain your design

1. Why does the Reactor own request metrics while worker exceptions are returned through the completion queue?
2. Why count a response only after all serialized bytes have been written?
3. Why are active connections and active jobs gauges rather than cumulative counters?
4. Why redact query strings and omit request headers/bodies from default access logs?
5. Why must a custom log sink be non-blocking and copy the supplied view if it retains it?
6. Why is the metrics snapshot thread-safe but not transactionally consistent across all fields?

## Learner task — `w8.http-observability`

**Workspace files**

- `include/http_server.hpp`
- `src/http_server.cpp`

Starting from D6-D1, add the public log and metrics API, JSONL access/error/lifecycle events, query-string redaction, a custom log sink, exception isolation, and atomic counters/gauges. Preserve the Reactor as the owner of connection state and metric updates.

**Fixed acceptance:** 8 platform-owned loopback GoogleTests in `course/fixtures/w8/http-observability`. They verify log fields, redaction, metrics, handler failures, sink exception isolation, timeout counters, connection rejection and queue pressure.

```bash
./build/cppapplab start w8.http-observability "$HOME/cppapplab-work/w8-observability"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-observability"
./build/cppapplab hint "$HOME/cppapplab-work/w8-observability" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-observability"
```
