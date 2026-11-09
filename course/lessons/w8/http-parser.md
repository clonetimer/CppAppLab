# W8-D2 · Incremental HTTP Request Parser

## Goal

Convert an arbitrary TCP byte buffer into a complete `HttpRequest` without assuming one `recv()` equals one HTTP request. The parser must distinguish incomplete input from malformed input and report exactly how many bytes belong to the first complete request.

## Scope from the course material

This learning parser supports:
- HTTP/1.0 and HTTP/1.1 request lines
- case-insensitive headers stored with lowercase names
- Content-Length-framed request bodies
- HTTP/1.1 Host requirement
- strict decimal Content-Length parsing
- explicit parser states: `NeedMoreData`, `Complete`, `Error`
- consumed-byte reporting for multiple requests in one TCP buffer

This learning parser deliberately rejects or does not implement:
- `Transfer-Encoding` (including chunked transfer)
- duplicate headers
- HTTP/2
- multipart parsing
- compressed bodies

The duplicate-header rejection is a conservative learning policy, not a universal rule for all HTTP headers.

## Parser contract

```cpp
enum class ParseStatus {
    NeedMoreData,
    Complete,
    Error
};

struct ParseResult {
    ParseStatus status;
    HttpRequest request;
    std::size_t consumed;
    std::string error;
};

class HttpRequestParser {
public:
    ParseResult parse(std::string_view buffer) const;
};
```

- `NeedMoreData`: no complete header terminator yet, or the declared body has not fully arrived.
- `Complete`: one full request is available; `consumed` is exactly the first request's byte length.
- `Error`: malformed or unsupported request, duplicate header, invalid Content-Length or resource limit exceeded.

## Required parsing order

1. Find `\r\n\r\n`. If missing, return NeedMoreData unless the header buffer exceeds 16 KiB.
2. Enforce the 16 KiB header limit, including the terminator.
3. Parse exactly three request-line tokens: method, target and version. Support HTTP/1.0 and HTTP/1.1 only.
4. Parse each `Name: Value` header line, validate the field name as an HTTP token with no whitespace before the colon, trim optional whitespace around the value, lowercase the name and reject duplicates.
5. Require Host for HTTP/1.1.
6. Reject Transfer-Encoding.
7. Parse Content-Length with `std::from_chars`; require the entire value to be consumed.
8. Enforce the 1 MiB body limit and guard the header-plus-body size calculation.
9. If fewer than Content-Length body bytes are present, return NeedMoreData.
10. Copy exactly the declared body bytes, return Complete and set consumed.

The parser must not consume a following pipelined request. The caller can erase the first `consumed` bytes and parse again.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-parser --help
cat request.http | ./build/http-parser parse-stdin
```

## What the platform checks

The fixed fixture tests complete GET/POST requests, partial headers and bodies, pipelining, consumed-byte reporting, HTTP version and Host validation, duplicate headers, Transfer-Encoding rejection, strict Content-Length, header/body limits, UTF-8 byte counts and body data containing CRLFCRLF.

## Explain your design

1. Why is NeedMoreData different from Error?
2. Why must the parser find CRLFCRLF before parsing headers?
3. Why does the parser return consumed bytes?
4. Why is Content-Length parsed with from_chars rather than stoi?
5. Why does the learning parser reject Transfer-Encoding?
6. Why are header names normalized to lowercase?
7. Why does the parser reject duplicate headers in this learning version?
8. Why are header and body size limits required?
9. Why can a single input buffer contain two complete requests?
10. Why should the caller loop until NeedMoreData?

## Acceptance

- [ ] Complete GET and POST requests parse correctly.
- [ ] Partial headers and bodies return NeedMoreData.
- [ ] Malformed requests return Error with a diagnostic.
- [ ] HTTP/1.1 requires Host; HTTP/1.0 does not.
- [ ] Duplicate headers and Transfer-Encoding are rejected.
- [ ] Content-Length is strict and byte-based.
- [ ] Header and body size limits are enforced.
- [ ] consumed reports only the first request.
- [ ] All 39 acceptance checks pass.
