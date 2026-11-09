# W7-A · TCP/IP and the First Blocking Echo Server

## Goal

Extend the W5 file-descriptor model to a real network endpoint. A Linux TCP socket is also represented by a file descriptor:

```text
socket → fd → send/recv → close
```

The application sees TCP as a reliable, ordered, bidirectional byte stream. TCP does **not** preserve application message boundaries: one `send()` is not guaranteed to correspond to one `recv()`.

## Topics

- IPv4, IP address, port, loopback and network byte order
- `socket / bind / listen / accept`
- `connect / send / recv / shutdown / close`
- `sockaddr_in`, `htons`, `htonl`, `inet_pton`
- blocking system calls and peer EOF
- partial sends, EINTR and RAII ownership

## Your implementation

Implement:

- `include/tcp_echo.hpp`: `TcpEchoServer`, `connect_tcp`, and `send_all`
- `src/tcp_echo.cpp`: IPv4 socket setup, blocking accept, echo loop and complete writes
- `src/main.cpp`: `server` and `client` command modes
- `tests/tcp_echo_test.cpp`: ten learner-authored tests

The server binds an IPv4 address and port, listens, accepts one client, and echoes bytes until the peer performs an orderly write shutdown. Port zero asks the OS to select an available port; query the actual port with `TcpEchoServer::port()`.

## Required semantics

- Validate the IPv4 literal and positive listen backlog.
- Use `AF_INET` and `SOCK_STREAM`.
- Convert ports with `htons`; do not write host-order values into `sin_port`.
- Handle interrupted `accept`, `recv`, and `send` calls.
- Handle partial `send` results until the full buffer is written.
- Treat `recv() == 0` as orderly peer shutdown.
- Close every owned descriptor on success and failure paths.
- Do not assume that one `recv` returns one complete message.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Terminal 1
./build/tcp-echo server 8080

# Terminal 2
./build/tcp-echo client 8080 "hello over TCP"
```

## What the platform checks

The fixed fixture runs 20 platform-owned behavior tests, ten learner tests and one CLI smoke test: 31 checks total.

Coverage includes socket setup, invalid addresses/backlog, ephemeral ports, loopback connection, byte-for-byte echo, binary payloads, large payloads, split writes, peer shutdown, and complete send behavior. The platform uses its own CMake fixture and declaration header; learner build files and tests are not the acceptance authority.

## Explain your design

1. Why does `accept()` return a new descriptor instead of reusing the listening descriptor?
2. Why is `htons(8080)` required?
3. What is the difference between `recv() == 0` and `recv() == -1`?
4. Why can `send()` write fewer bytes than requested?
5. Why is TCP a byte stream rather than a message protocol?
6. Who owns each descriptor, and where is it closed?
7. Why is a blocking `accept()` suitable for this first exercise but not a high-connection-count server?

## Acceptance

- [ ] Server binds and listens on an IPv4 endpoint.
- [ ] Client connects using `connect()`.
- [ ] Server accepts a client and echoes all received bytes.
- [ ] Binary and large payloads are preserved exactly.
- [ ] Peer write shutdown ends the echo session cleanly.
- [ ] Partial sends and EINTR are handled.
- [ ] Descriptors are closed on all normal paths.
- [ ] All 31 fixed checks pass.
