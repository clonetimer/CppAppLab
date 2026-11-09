# W7-B · Reference Multi-client TCP Chat

Build and test on Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/tcp-chat server 9000
```

Connect several terminals with `nc 127.0.0.1 9000`. Send `NICK alice`, then `MSG hello`, and leave with `QUIT`.

The server uses non-blocking IPv4 sockets, level-triggered epoll, one event-loop owner, per-client input/output buffers, bounded client and buffer sizes, and eventfd-based stop wakeup. The protocol is newline-delimited and broadcasts messages to other registered clients.
