# W8-D3 · HTTP Responses and Static Files

## Goal

Connect the W8-D2 parsed request model to an HTTP response and a static-file handler:

```text
HttpRequest
    ↓
StaticFileHandler
    ↓
HttpResponse
    ↓
serialize_response()
    ↓
HTTP/1.1 bytes
```

This task builds a handler and serializer demonstration. It does not yet open a listening TCP socket; the networking/event-loop integration is a later stage.

## Scope from the course material

- Return `index.html` for `GET /`.
- Read static files using `std::ios::binary`, preserving exact bytes.
- Select Content-Type by extension: html, css, js, txt, json, png, jpg/jpeg and svg; use `application/octet-stream` otherwise.
- Return 404 for missing files.
- Allow GET only; return 405 and `Allow: GET` for other methods.
- Strip the query string before file lookup.
- Reject unsafe targets with 400.
- Keep file access and MIME selection inside `StaticFileHandler`, not in the TCP/epoll layer.
- Reuse the D1 response serializer so Content-Length is calculated from body bytes and Connection: close is emitted.

The path check is intentionally conservative: the learning implementation rejects targets that are not root-relative, contain `..`, or contain a backslash. This is not a production-grade document-root containment mechanism. URL decoding, canonical path checks and symlink containment remain known limitations.

## Architecture

- `HttpRequest` / `HttpResponse`: protocol data models from D1.
- `serialize_response`: validates response headers and emits status line, headers, CRLFCRLF, byte Content-Length and body.
- `StaticFileHandler`: maps a request to a file or an HTTP error response.
- `main.cpp`: one-request command-line demonstration; this is not a network server.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-static-files --help
./build/http-static-files handle GET /
./build/http-static-files handle GET /hello.txt
./build/http-static-files handle POST /
```

The command-line example uses a `www/` directory relative to the current working directory.

## What the platform checks

The fixed fixture covers root-to-index mapping, binary file reads, MIME types, unknown extensions, 404, 405/Allow, unsafe paths, query handling, Content-Length and Connection: close. Platform-owned headers, tests and CMake define the acceptance boundary.

## Explain your design

1. Why is `HttpResponse` a separate model from `HttpRequest`?
2. Why should the serializer calculate Content-Length?
3. Why must static files be read in binary mode?
4. What does Content-Type tell the client?
5. Why does `/` normally map to `/index.html`?
6. When should the handler return 404 and 405?
7. Why must a request target not be concatenated directly into a local path?
8. What is path traversal?
9. Why should epoll/network code not perform file I/O and MIME selection?
10. Which layer owns parsing, static-file handling, response serialization and socket I/O?

## Acceptance

- [ ] GET / returns index.html.
- [ ] Text and binary files preserve their exact bytes.
- [ ] Supported extensions return the expected MIME type.
- [ ] Unknown extensions return application/octet-stream.
- [ ] Missing files return 404.
- [ ] Non-GET requests return 405 with Allow: GET.
- [ ] Unsafe targets return 400.
- [ ] Query strings do not affect file lookup.
- [ ] Content-Length counts body bytes and Connection: close is emitted.
- [ ] All 31 acceptance checks pass.
