# W8-D4 — Reference Router and Dynamic HTTP Handlers

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
printf 'GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello' | ./build/http-router parse-stdin
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin www
```

This is the complete reference solution for the handler pipeline, not a listening HTTP server. The platform-owned fixture runs 32 fixed tests, these ten reference tests as learner submissions, and a byte-exact CLI smoke test: 43 total.

D1/D2/D3 dependency source is carried forward without modification. Their documented limitations, including unsupported Transfer-Encoding and conservative static-path checks, remain in force. Register routes before serving; do not mutate registrations concurrently with dispatch.
