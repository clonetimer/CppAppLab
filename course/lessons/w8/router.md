# W8-D4 — Router and Dynamic HTTP Handlers

## Goal and source

Based on the supplied `快速C++` D4 section, separate dynamic HTTP handlers from static-file lookup. Keep the course's progression: request model → Router/StaticFileHandler → response model → serializer. This task implements the application pipeline independently of sockets; D5 will integrate networking and a worker pool. The private `clonetimer/cpptest` repository returned 404 during this implementation, so no claims are made about incorporating its source.

## Course-derived behavior

Router matches **method + path**, not path alone. Its callback type is `HttpResponse(const HttpRequest&)`, stored with `std::function`. Re-registering the same method/path replaces the callback. Matching removes the query suffix, but the original request, including target, headers and body, reaches the handler unchanged.

| Endpoint | Response body | Content-Type |
|---|---|---|
| GET /hello | `Hello from C++ HTTP Server!` followed by LF | text/plain; charset=utf-8 |
| POST /echo | Exact request body bytes | text/plain; charset=utf-8 |
| GET /health | `{"status":"ok"}` | application/json |

Unknown dynamic paths go to the existing static-file handler. GET / still returns index.html; missing resources and unsupported static methods retain D3 behavior. No external JSON library is introduced.

## Explicit engineering choices

The source first presents exact-route fallback and later discusses `hasPath`/405 as a refinement. This task adopts the latter consistently: a known dynamic path owns method mismatches and handler-generated errors. It must not fall through to disk merely because the callback returned 404 or the request used another method.

- Missing dynamic path in `Router::handle`: 404. Known path but unmatched method: 405 plus sorted, unique `Allow` methods.
- Registration validates a nonempty HTTP method token, a leading-slash visible-ASCII path without query/fragment/backslash, and a nonempty callback. Invalid input is rejected before replacing a valid entry.
- Storage is a nested path/method map rather than the source's concatenated key; matching remains exact and case-sensitive.
- `has_route` and `has_path` take already-extracted paths. `handle` and `dispatch_request` strip the query for lookup only.
- Handler exceptions propagate to the application boundary; the demonstration CLI reports them. Route registration must finish before serving. Concurrent mutation is not part of this task.

These are documented task-contract refinements, not claims that every detail appeared in the source example.

## Implementation files

Implement `router.cpp`, `api_handlers.cpp` and `dispatcher.cpp`, and ten learner tests. Six public headers declare the model, parser, static handler, router, API handlers and dispatcher. Working D1/D2/D3 implementation files are supplied as scaffolding and remain included in the source fingerprint.

The dispatcher uses path existence rather than the returned status code to choose the dynamic handler. The serializer alone constructs wire bytes and body-byte Content-Length.

## Build and exercise

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
printf 'GET /hello?name=Alice HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello' | ./build/http-router parse-stdin
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build/http-router parse-stdin www
```

`parse-stdin [document-root]` reads exactly one bounded request until EOF. It uses the actual hardened D2 parser. An incomplete/malformed request returns 400 and process exit 1; trailing requests or an input above 16 KiB + 1 MiB are rejected with exit 2. The parser still independently enforces its header/body limits. A valid HTTP error response such as 404 or 405 is a successful CLI execution, not a process failure.

**This CLI does not bind a TCP port.** Browser/curl-over-socket acceptance belongs to the subsequent network-server task. Query parameter parsing, URL decoding, pattern routes, TLS and chunked transfer are not added here. Static-file canonical/symlink containment remains a documented D3 limitation.

## Fixed acceptance

32 platform-owned GoogleTest cases + 10 learner tests + 1 real CLI integration smoke = **43** checks. The smoke compares complete serialized Hello and binary Echo responses, checks health, static fallback, 405 with Allow, and rejection of the malformed D2 header `Host : value`. The root workflow deliberately replaces learner CMake with a fatal error to prove that `cppapplab check` uses its fixed fixture.

## Explain your design

Why must method and path both participate in lookup? Why does the handler receive the original target? How does binary Echo use Content-Length? Why must a dynamic 404 remain a dynamic response? How do 404 and 405 differ? Why must a known dynamic path not fall through to a same-named file? Which layer owns socket I/O, parsing, dispatch, disk access and serialization?

## Acceptance

- [ ] Exact method/path routing and duplicate replacement work.
- [ ] Query stripping affects lookup only.
- [ ] Hello, binary/UTF-8 Echo and JSON Health match the course outputs.
- [ ] Known dynamic paths return 405 with deterministic Allow for wrong methods.
- [ ] Static fallback remains functional and dynamic errors never trigger disk fallback.
- [ ] Actual parser/dispatcher/serializer CLI integration passes.
- [ ] All 43 checks pass without using learner-controlled build files as acceptance authority.
