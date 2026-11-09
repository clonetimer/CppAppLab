# W8-D1 · HTTP/1.1 Protocol Foundations

## Goal

Build the application-protocol layer on top of the TCP byte stream completed in W7. This lesson covers HTTP/1.1 message structure, request and response lines, headers, status codes, byte-accurate Content-Length, CRLF framing, and the distinction between a protocol message and a TCP receive operation.

This task deliberately does not implement a full HTTP request parser, TLS, HTTP/2 or HTTP/3. Incremental request parsing is the next W8 stage.

## Topics from the course material

### Request structure

An HTTP request consists of:

1. Request line: `METHOD target VERSION`
2. Headers in `Name: Value` form
3. An empty line, represented by `\r\n\r\n` after the final header line
4. An optional body

Examples:

```http
GET /hello HTTP/1.1
Host: 127.0.0.1:8080
Accept: */*

```

```http
POST /echo HTTP/1.1
Content-Type: text/plain
Content-Length: 5

hello
```

### Response structure

A response consists of a status line, headers, an empty line and an optional body:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, world!
```

The request line is `METHOD target VERSION`; the response status line is `VERSION status reason`.

### Status codes

- 200 OK, 201 Created, 204 No Content
- 301 Moved Permanently, 302 Found
- 400 Bad Request, 401 Unauthorized, 403 Forbidden
- 404 Not Found, 405 Method Not Allowed
- 500 Internal Server Error, 503 Service Unavailable

A valid request for a missing resource is 404; a malformed request line is 400. 401 means authentication is missing or invalid; 403 means the identity is known but lacks permission.

### Framing and connection behavior

HTTP/1.1 text framing uses CRLF (`\r\n`), not a lone LF. The header terminator is `\r\n\r\n`. Content-Length counts body bytes, not Unicode characters. One `recv()` is not guaranteed to contain a complete HTTP request.

HTTP/1.1 normally supports persistent connections. This first implementation explicitly emits `Connection: close` to keep connection lifecycle simple; keep-alive is a later stage.

## Implementation files

- `include/http_message.hpp`: `HttpRequest` and `HttpResponse`
- `src/http_message.cpp`: request-line parsing and response serialization
- `src/main.cpp`: small protocol demonstration CLI
- `tests/http_message_test.cpp`: ten learner-authored tests

## Required behavior

- Parse exactly three request-line tokens: method, target and version.
- Reject missing fields, extra tokens, empty input and embedded line breaks.
- Preserve any existing headers and body when parsing only the request line.
- Serialize an HTTP/1.1 status line, headers, CRLFCRLF and body.
- Calculate Content-Length from the final body's byte count.
- Emit `Connection: close` for this first server stage.
- Treat header names as case-insensitive; reject duplicate names that differ only by case.
- Reject malformed header names and CR/LF injection in header values or reason phrases.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-protocol parse-line "GET /hello HTTP/1.1"
./build/http-protocol response
```

You can inspect real HTTP exchange details with `curl -v http://127.0.0.1:8080/` when a server is available, or manually type a request into `nc 127.0.0.1 8080`. Remember the final blank line.

## What the platform checks

The fixed fixture checks request-line parsing, malformed input, preservation of request fields, status-line formatting, header handling, byte-accurate Content-Length, forced close semantics, deterministic header order and header injection protection. The platform fixture owns its declaration header, tests and CMake configuration.

## Explain your design

1. What is the relationship between HTTP and TCP?
2. Which three fields make up the HTTP request line?
3. Which three fields make up the response status line?
4. What is the basic header format?
5. What does Content-Length measure?
6. Why does HTTP/1.1 use CRLFCRLF to end the headers?
7. Why can one recv() not be treated as a complete request?
8. What do 200, 400, 404 and 500 mean?
9. How do 401 and 403 differ?
10. What is the difference between Connection: close and keep-alive?
11. Why will the full HTTP parser need a buffer and parser state?
12. Why keep HttpRequest and HttpResponse separate from transport code?

## Acceptance

- [ ] Request line parses into method, target and version.
- [ ] Missing fields and extra tokens are rejected.
- [ ] Existing headers and body survive request-line parsing.
- [ ] Response uses HTTP/1.1 status line and CRLF framing.
- [ ] Content-Length counts bytes.
- [ ] Connection: close is emitted.
- [ ] Header names are handled case-insensitively.
- [ ] Header and reason-phrase injection is rejected.
- [ ] All 32 fixed checks pass.
