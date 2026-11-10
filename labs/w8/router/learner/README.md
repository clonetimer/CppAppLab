# W8-D4 — Router and Dynamic HTTP Handlers

Implement the TODOs in `src/router.cpp`, `src/api_handlers.cpp`, `src/dispatcher.cpp`, and the ten learner tests. The working D1 serializer, hardened D2 parser and D3 static handler are carried forward as scaffolding; keep their interfaces and behavior.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
printf 'GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin
```

The CLI accepts one complete HTTP request from standard input and prints the serialized response. `parse-stdin [document-root]` defaults to `www`. It does not open a listening socket. Use the provided `www/index.html` for the static fallback demonstration.

The assessed contract includes exact method/path matching, query stripping for matching only, 404/405 with deterministic Allow, binary echo and dynamic/static dispatch. Register routes before serving; concurrent mutation is not supported. See the lesson for the source-derived scope and explicit engineering refinements.
