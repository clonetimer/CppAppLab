# W7-B · Multi-client TCP Chat with Non-blocking I/O and epoll

## Goal

Build the W7 completion project: a multi-client TCP chat server using non-blocking Linux sockets and an `epoll` event loop.

W7-A already introduced the blocking socket lifecycle. This task moves from one blocking connection to many concurrent connections without assigning one operating-system thread to every client.

## Learning progression

Study the alternatives in this order before implementing the final event loop:

1. **Thread-per-connection:** easy to reason about, but every connection consumes a thread and shared broadcast state needs synchronization.
2. **Non-blocking sockets:** `O_NONBLOCK` changes would-block operations into `EAGAIN/EWOULDBLOCK`; the application must retain state and resume later.
3. **select/poll:** readiness multiplexing lets one thread manage multiple descriptors, but `select` has descriptor-set limits and `poll` scans the descriptor array.
4. **epoll:** Linux readiness notification scales better for a large descriptor set. This task uses level-triggered epoll and drains readable/writable work until `EAGAIN`.

The final deliverable is an epoll server. The earlier models are comparative learning stages, not separate production implementations.

## Chat protocol

The server accepts newline-delimited UTF-8/byte text commands:

- `NICK <name>\n` registers a nickname.
- `MSG <text>\n` broadcasts a message from a registered user.
- `QUIT\n` disconnects the client.

Server broadcasts:

- `* <name> joined\n`
- `<name>: <text>\n`
- `* <name> left\n`

Names are 1–24 ASCII letters, digits, underscore or hyphen. Nicknames are unique while connected. A client must register before sending `MSG`.

## Required architecture

- non-blocking IPv4 listening socket
- `epoll_create1` and level-triggered readiness
- `accept4` with `SOCK_NONBLOCK | SOCK_CLOEXEC`
- one event-loop thread owns the client map and all client state
- per-client input buffer for fragmented lines
- per-client output queue for partial writes
- `EPOLLIN` enabled while reading is allowed; `EPOLLOUT` enabled only while output is pending
- `eventfd` wakeup for a stop request from another thread
- bounded client count, line length and pending output bytes
- close-on-error and graceful cleanup

Do not block the event loop on a client. Read and write until the operation would block, then return to epoll.

## Implementation files

- `include/tcp_chat.hpp`: `ChatServerConfig` and `ChatServer` API
- `src/tcp_chat.cpp`: socket setup, epoll loop, protocol, connection state and buffering
- `src/main.cpp`: server CLI
- `tests/tcp_chat_test.cpp`: ten learner-authored tests

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/tcp-chat server 9000
```

Connect from multiple terminals using `nc 127.0.0.1 9000`, then enter `NICK alice`, `MSG hello` or `QUIT`.

## What the platform checks

The fixed acceptance fixture validates the learner implementation, not learner-controlled build files or tests. It covers nickname validation and uniqueness, registration requirements, message broadcast, fragmented input, multiple clients, partial output, disconnect cleanup, bounded resources, stop wakeup and event-loop lifecycle.

## Explain your design

1. Why must an edge-triggered epoll loop drain a descriptor until `EAGAIN`? How does level-triggered mode differ?
2. Why should `EPOLLOUT` be enabled only when there is pending output?
3. Why does TCP require a per-client input buffer even when the protocol is newline-delimited?
4. What happens if a slow client cannot drain its output queue?
5. Why does one event-loop thread simplify ownership of the client map?
6. How does `eventfd` allow another thread to request shutdown without closing descriptors concurrently?
7. Compare thread-per-connection, select, poll and epoll in terms of scheduling, descriptor management and scaling.

## Acceptance

- [ ] Multiple clients can register concurrently.
- [ ] Nicknames are validated and unique.
- [ ] Messages are broadcast to all other registered clients.
- [ ] Fragmented and coalesced TCP input is parsed correctly.
- [ ] Partial writes are buffered and resumed on EPOLLOUT.
- [ ] Non-blocking read/write loops stop on EAGAIN/EWOULDBLOCK.
- [ ] Slow clients cannot grow memory without bound.
- [ ] Disconnects remove epoll registrations and client state.
- [ ] Stop requests wake and terminate the event loop.
- [ ] All fixed acceptance checks pass.
