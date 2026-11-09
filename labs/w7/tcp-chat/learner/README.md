# W7-B · Multi-client TCP Chat

This task extends the blocking TCP Echo Server from W7-A into a non-blocking multi-client service.

The final implementation uses Linux `epoll` and a single event-loop owner for all client state. The lesson compares thread-per-connection, `select`, `poll` and `epoll`, but the assessed implementation is the epoll server.

Build after implementing the TODOs:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/tcp-chat server 9000
```

Connect multiple clients with `nc 127.0.0.1 9000`. Register with `NICK <name>`, chat with `MSG <text>`, and leave with `QUIT`.

The server must use non-blocking sockets, newline framing, per-client output buffers, bounded resource usage and an eventfd stop channel. Do not block the event loop on one slow client.
