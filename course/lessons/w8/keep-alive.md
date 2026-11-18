# W8-D6-B · Sequential HTTP Keep-Alive Lifecycle

## Start here

Make two sequential requests over one connection.

1. Connect once and send a complete request with keep-alive.
2. Read the complete response, including its body boundary.
3. Send a second request on the same socket.
4. Verify the server parses exactly one request at a time and retains any following bytes.
5. Test a request that asks to close and verify the connection closes after its response.
6. Test pipelined bytes without allowing concurrent response reordering.

**Mental model:** a persistent connection is a state machine that reuses one socket while preserving request/response order.


## Goal

Use the D6-A request policy to reuse a TCP connection safely. The Reactor remains the sole owner of sockets and connection state. A connection may have only one request executing or one response being written at a time.

## Connection state transition

1. Receive bytes into the connection read buffer.
2. Parse exactly one complete request and remove only its consumed prefix.
3. Compute the request's connection policy and retain the response protocol version.
4. Submit the owned request to the W6 worker pool.
5. Serialize the response with the saved policy and protocol version.
6. Disable further reads while the response is pending; finish partial writes through EPOLLOUT.
7. After the complete response is sent:
   - close when policy is Close;
   - otherwise parse the next request already buffered;
   - if no complete request is buffered, re-enable EPOLLIN and wait for more bytes.

The next request is not dispatched until the previous response has been completely written. This preserves response order without requiring concurrent pipelined handler execution.

## Buffer and ownership rules

- The event-loop thread exclusively owns Connection, socket descriptors, epoll registrations, and read/write buffers.
- Workers receive HttpRequest values and return HttpResponse values only.
- The parser's consumed byte count removes one request prefix while retaining any pipelined bytes.
- EPOLLIN is disabled while a request is processing or a response is being written.
- EPOLLOUT is enabled only while response bytes remain.
- The D6-A connection policy is saved before the request is moved to a worker.
- The response status line uses the request's HTTP/1.0 or HTTP/1.1 version.
- Explicit close discards subsequent pipelined bytes after the current response.

## Peer half-close

A client may send one or more complete requests and then shut down its write side. The server drains the received bytes and processes complete buffered requests sequentially. It closes after the final buffered request, even if that last request would otherwise default to Keep-Alive.

An incomplete request at EOF receives 400 and closes. A pipeline exceeding the configured read-buffer limit is rejected rather than allowing unbounded accumulation.

## D5 compatibility

The D5 acceptance fixture and CLI smoke explicitly send Connection: close. This preserves the D5 one-request regression contract while D6-B adds persistent-connection behavior in a separate fixed fixture.

## Troubleshooting

The fixed loopback checks drive this library task; use their failing case names to trace connection state.

- **The second request hangs:** inspect the retained read buffer, parser `consumed` count and EPOLLIN state after the first full response. Recheck sequential and fragmented requests, including bytes already buffered before reads resume.
- **Pipelined responses arrive out of order:** compare the next handler's dispatch time with completion of the current response write. Recheck the ordering case with only one request processing or response writing per connection.
- **HTTP/1.0 gets the wrong version or reuse policy:** inspect the policy and version saved before moving the request to a worker. Recheck both HTTP/1.0 cases and explicit close, which discards later pipelined requests after its response.
- **A peer half-close loses valid requests:** inspect EOF separately from buffered request completeness. Recheck pipeline draining; complete buffered requests still receive responses, while an incomplete request at EOF receives 400 and closes.

## Acceptance

The dedicated loopback fixture covers:

1. Sequential requests on one socket.
2. Pipelined requests and response order.
3. Explicit close discarding later pipelined requests.
4. A fragmented subsequent request.
5. HTTP/1.0 default close and response version.
6. HTTP/1.0 explicit Keep-Alive opt-in.
7. Draining a complete pipeline after peer half-close.
8. Preventing the next handler from starting before the current response is ready.

Final-head CI run `42971176071` passed all three jobs. The eight loopback cases passed 8/8 under GCC, ASan/UBSan and clang-tidy. Root GCC/CTest and root ASan/UBSan each passed 53/53. The existing D5 fixed fixture remains a separate regression gate.

## Deliberate D6-C boundary

This stage does not yet add idle/header/body deadlines, a per-connection request-count cap, or global persistent-connection quotas. Those controls are D6-C. Until then, the current max-connections and bounded read/write/worker limits remain the resource controls.

## Learner task — `w8.http-keep-alive`

**Workspace files**

- `include/http_server.hpp`
- `src/http_server.cpp`

The workspace includes the D5 Reactor baseline. Implement the D6-A policy integration and sequential persistent-connection lifecycle: preserve request policy/version, retain pipelined bytes, finish each response before dispatching the next request, and drain complete requests after a peer half-close.

**Fixed acceptance:** 8 platform-owned loopback GoogleTests in `course/fixtures/w8/http-keep-alive`. They cover sequential reuse, pipelining, close semantics, fragmented requests, HTTP/1.0 behavior, half-close draining and response ordering.

The Router/protocol and ThreadPool dependencies are supplied by the platform fixture; the learner's server header and implementation are the only staged implementation files. The D6-A policy is a known prerequisite baseline for this task.

```bash
./build/cppapplab start w8.http-keep-alive "$HOME/cppapplab-work/w8-keep-alive"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-keep-alive"
./build/cppapplab hint "$HOME/cppapplab-work/w8-keep-alive" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-keep-alive"
```
