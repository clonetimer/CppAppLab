# W8-D6-D3 · Operational Load, Backpressure and Soak Validation

## Goal

Validate the operational behavior of the HTTP server under concurrent clients, slow readers, sustained Keep-Alive traffic and repeated lifecycle transitions. These tests are intended to expose resource leaks, stalled writes, queue-accounting errors and regressions across the complete D5/D6 Reactor pipeline.

## Fixed loopback acceptance

### 1. Concurrent clients and worker bounds

A burst of independent loopback clients sends requests concurrently through the real listener and Reactor.

The fixture verifies:

- every request receives its complete expected response
- handler concurrency is greater than one and never exceeds the configured worker count
- bounded outstanding-job capacity is not exceeded
- no accepted connection, worker job or response is lost
- active connection and active-job gauges return to zero

The test does not require a particular requests-per-second value. Scheduling and shared-runner performance vary.

### 2. Slow-reader TCP backpressure

A route returns an 8 MiB response while the client deliberately constrains its receive buffer and delays reading.

The fixture verifies:

- the server can stop writing when the kernel send buffer is full
- EPOLLOUT resumes delivery when the peer starts reading
- the complete body arrives byte-for-byte
- the server records the response only after the complete serialized response has been written
- connection and worker gauges recover afterward

This exercises the actual non-blocking socket path; it is not a mock transport test.

### 3. Sustained Keep-Alive soak

Eight concurrent clients each send forty sequential requests over one persistent TCP connection, then explicitly close it.

The fixture checks all 320 responses, response ordering, zero queue rejections, zero protocol errors, and eventual recovery of active connections, persistent-connection slots and worker jobs.

The soak is intentionally bounded for CI repeatability. It is a regression soak, not a production capacity benchmark or a multi-hour endurance claim.

### 4. Repeated server lifecycle

The fixture creates, runs, serves, stops and joins eight independent server instances. It requests stop twice per instance to verify idempotence, then checks that connections and worker jobs are drained before destruction.

## Interpretation and limits

- CI validates deterministic correctness and resource recovery, not production throughput, tail latency or a capacity SLA.
- The custom Router handler is executed on the bounded worker pool. Handlers must remain bounded/cooperative; arbitrary callbacks cannot be forcibly cancelled.
- The server remains Linux-specific. Hosted Ubuntu CI is not a substitute for Windows 11 + WSL2 target-machine acceptance.
- Slow-reader behavior is covered at the TCP send-buffer boundary. It does not establish TLS, HTTP/2, chunked transfer or production-grade load-balancer behavior.

## Explain your design

1. Why is worker concurrency bounded independently of accepted TCP connections?
2. How does a slow reader create backpressure in a non-blocking TCP server?
3. Why must EPOLLOUT be enabled only while serialized response bytes remain?
4. Why does a Keep-Alive soak reveal resource-accounting defects that one-request-per-connection tests can miss?
5. Why are fixed functional load tests not sufficient to claim a throughput or production-capacity SLA?
6. Which metrics would you collect in a separate benchmark to report throughput, p50/p95/p99 latency, queue wait and error rate?
