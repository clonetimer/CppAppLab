# W8-D1 · HTTP Protocol Foundations

This task introduces HTTP/1.1 message structure above the TCP byte stream. Implement request-line parsing and response serialization; full incremental parsing is W8-D2.

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Try:

```bash
./build/http-protocol parse-line "GET /hello HTTP/1.1"
./build/http-protocol response
```

The first response serializer always uses `Connection: close`. It computes Content-Length from body bytes and rejects malformed header names and CR/LF injection.
