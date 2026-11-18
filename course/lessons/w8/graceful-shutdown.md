# W8-D6-D1 · Graceful HTTP Server Shutdown

## Start here

Shutdown is a coordinated state transition, not just killing a process.

1. Start the server and open one idle client connection.
2. Request shutdown and observe how the Reactor is woken.
3. Verify the listening socket stops accepting new clients.
4. Observe how idle connections and active requests are handled.
5. Confirm worker threads finish or are bounded by the shutdown contract.
6. Repeat shutdown to check idempotency and descriptor cleanup.

**Mental model:** the Reactor owns sockets and connection state. Workers must not independently close Reactor-owned descriptors during shutdown.


## Goal

Stop accepting new clients, finish already accepted work where possible, and close sockets deterministically. Shutdown must be driven by the Reactor thread; worker threads must never close client descriptors or mutate Connection state.

## Shutdown sequence

1. `request_stop()` sets the stop flag and wakes the Reactor through eventfd.
2. The Reactor removes and closes the listening socket. No further clients are accepted.
3. Idle Keep-Alive connections are closed immediately.
4. Connections with incomplete request bytes receive HTTP 503 Service Unavailable with Connection: close.
5. Requests already executing in the worker pool are allowed to finish. Their pending response policy is changed to Close, so the completed response does not advertise another reusable request.
6. Responses already being written are allowed to finish and the connection is closed afterward. If no response bytes have been sent, the Connection header is rewritten to close before writing.
7. A monotonic shutdown deadline bounds how long sockets may remain open. When it expires, remaining connections are closed.
8. The Reactor continues draining worker completions. It exits after connections are gone and all submitted worker jobs have completed.

## Configuration

`HttpServerConfig::shutdown_timeout` defaults to 5 seconds and follows the same validation rule as other deadlines: positive and no greater than 24 hours.

The shutdown deadline bounds connection draining, not arbitrary user-handler execution. The existing ThreadPool cannot forcibly cancel a C++ callback that never returns. Handlers must therefore be bounded/cooperative; the server waits for submitted jobs to complete before `run()` returns and the pool is joined.

## Response semantics

- In-flight requests receive their normal handler response with Connection: close.
- Incomplete requests receive 503 and close.
- Idle Keep-Alive connections close silently.
- A response already partially written is completed if possible, then the socket is closed. Once part of a header has reached the peer, the server cannot safely rewrite the bytes already sent; TCP EOF still terminates the connection.

## Troubleshooting

The fixed loopback checks call the library's stop/join lifecycle; this task does not provide a separate server executable.

- **Stop leaves the Reactor asleep or still accepting:** inspect `request_stop()`, its eventfd wakeup and the Reactor's listener cleanup. Recheck the idle-connection case with the shutdown transition performed by the socket-owning thread.
- **Incomplete and in-flight requests get the same shutdown result:** inspect whether parsing completed and a worker owns the request. Recheck all three states: idle closes silently, incomplete bytes receive 503, and accepted handler work may finish normally.
- **An unsent response still advertises reuse:** inspect its pending connection policy and write offset at shutdown. Recheck forced-close handling before bytes are sent; a partially sent header cannot be rewritten, so close after draining the response where possible.
- **`run()` outlasts `shutdown_timeout`:** inspect remaining sockets and submitted worker jobs separately. Recheck the deadline case with a bounded handler; the deadline closes sockets, while completions must still drain and workers must finish before joining.

## Acceptance

The fixed loopback fixture covers:

1. Closing an idle Keep-Alive connection on stop.
2. Draining an in-flight handler and returning its response with Connection: close.
3. Returning 503 for an incomplete request during shutdown.
4. Closing a connection at the configured shutdown deadline while allowing its bounded handler to finish.
5. Rejecting a zero shutdown timeout.

The fixture runs under GCC, ASan/UBSan and clang-tidy. Existing D5, D6-B and D6-C fixtures remain separate regression gates.

## Explain your design

1. Why must the Reactor own listener/client descriptor shutdown?
2. Why are in-flight requests treated differently from idle connections?
3. Why is the shutdown deadline tracked by the same monotonic timerfd as connection deadlines?
4. Why can the server bound socket drain time but not forcibly cancel an arbitrary C++ handler?
5. Why must the server continue draining worker completions after closing a timed-out connection?

## Learner task — `w8.http-graceful-shutdown`

**Workspace files**

- `include/http_server.hpp`
- `src/http_server.cpp`

Starting from D6-C, implement an explicit shutdown deadline and a Reactor-owned shutdown state transition. Stop accepting new clients, close idle sockets, return 503 for incomplete requests, drain in-flight work with Connection: close, and close remaining sockets when the monotonic deadline expires. Continue draining worker completions before `run()` returns.

**Fixed acceptance:** 5 platform-owned loopback GoogleTests in `course/fixtures/w8/http-graceful-shutdown`.

The shutdown deadline bounds socket draining; it cannot forcibly cancel an arbitrary C++ handler. The worker pool must still be joined safely.

```bash
./build/cppapplab start w8.http-graceful-shutdown "$HOME/cppapplab-work/w8-graceful-shutdown"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-graceful-shutdown"
./build/cppapplab hint "$HOME/cppapplab-work/w8-graceful-shutdown" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-graceful-shutdown"
```
