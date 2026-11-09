# W8-D2 · Incremental HTTP Request Parser

Implement the parser states and framing logic in `src/http_request_parser.cpp`. The parser consumes an arbitrary buffer, not a socket descriptor. The HTTP server will own the TCP receive buffer and call the parser repeatedly.

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Try the CLI with a complete request piped through stdin:

```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-parser parse-stdin
```

The parser supports Content-Length framing only. Transfer-Encoding is explicitly rejected in this learning stage.
