# W7-A · TCP Echo Server

Build on Linux with GCC or Clang in C++17 mode:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the blocking server in one terminal:

```bash
./build/tcp-echo server 8080
```

Run a client in another terminal:

```bash
./build/tcp-echo client 8080 "hello over TCP"
```

The server accepts one client, echoes the TCP byte stream until the client shuts down its write side, and then exits. Port zero is supported by the library for tests; the kernel selects an available port.

This is the first blocking-socket exercise. It intentionally does not implement concurrency, non-blocking I/O, `select`, `poll`, or `epoll`; those are subsequent W7 stages.
