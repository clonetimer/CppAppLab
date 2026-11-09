# W7-B · Multi-client TCP Chat

Implement the TODOs in `src/tcp_chat.cpp` and replace the ten learner-test placeholders in `tests/tcp_chat_test.cpp`.

The target design is a single-owner Linux `epoll` event loop using non-blocking sockets, bounded per-client input/output buffers, newline-framed commands, and an `eventfd` stop channel. The lesson compares thread-per-connection, select, poll and epoll; the final assessed implementation is epoll.

Build after implementing the task:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/tcp-chat server 9000
```

Connect several terminals with `nc 127.0.0.1 9000`. Register using `NICK <name>`, send messages using `MSG <text>`, and disconnect using `QUIT`.
