# W8-D6-A · HTTP Connection Policy and Response Serialization

## Start here

First decide whether one request should keep its TCP connection open.

1. Compare HTTP/1.0 and HTTP/1.1 defaults.
2. Read the Connection header as a list of comma-separated options.
3. Implement exact, case-insensitive token matching; do not use substring matching.
4. Serialize the response with the matching Connection policy.
5. Test explicit close, keep-alive and conflicting options.
6. Confirm the server lifecycle itself has not changed in this lesson.

**Mental model:** connection policy is a decision derived from the request. The serializer communicates that decision; the next lesson applies it to the actual socket lifecycle.


## Goal

Define connection persistence from the HTTP request and make the response serializer emit the matching policy. This stage prepares the protocol layer for Keep-Alive without changing the TCP server lifecycle yet.

## Connection policy

The implementation recognizes the Connection field as a comma-separated, case-insensitive list of exact options. Optional whitespace around each option is ignored.

| Request version | Connection options | Result |
|---|---|---|
| HTTP/1.1 | absent or no close option | Keep-Alive |
| HTTP/1.1 | contains close | Close |
| HTTP/1.0 | absent or no keep-alive option | Close |
| HTTP/1.0 | contains keep-alive and no close | Keep-Alive |
| Either supported version | contains both keep-alive and close | Close |
| Unsupported version | any | Close |

Unknown connection options do not change the default. Matching is by complete token, not substring. The close option takes precedence.

The parser already normalizes parsed header names to lowercase. The policy helper also compares names case-insensitively so manually constructed requests have deterministic behavior.

## API

- `ConnectionPolicy::Close` and `ConnectionPolicy::KeepAlive` represent the decision.
- `request_connection_policy(const HttpRequest&)` computes the policy without modifying the request.
- `serialize_response(response, policy)` writes exactly one authoritative Connection header, defaults the status line to HTTP/1.1 and computes Content-Length from the body bytes.
- `serialize_response(response, policy, response_version)` preserves HTTP/1.0 or HTTP/1.1 in the status line; unsupported versions are rejected.
- The legacy one-argument `serialize_response(response)` overload remains close-after-response for W1–W8-D5 compatibility.

Caller-provided Connection and Content-Length values are not forwarded; the serializer owns both fields.

## Important boundary

D6-A does **not** yet reuse a TCP connection. The D5 server continues calling the legacy one-argument serializer and closes after one response. D6-B will connect the request policy to the Reactor's connection lifecycle, process buffered subsequent requests, and preserve response order.

Do not enable Keep-Alive in the server before the lifecycle changes are implemented.

## Troubleshooting

Use the task's fixed checks to exercise these library functions; this workspace has no standalone server command.

- **HTTP/1.0 and HTTP/1.1 choose the same default:** inspect the request's version before evaluating options. Recheck the no-header cases and HTTP/1.0 opt-in; unsupported versions must choose Close.
- **`x-close` closes the connection or mixed-case options are missed:** inspect comma-separated tokens, whitespace and case handling. Recheck exact-token cases and ensure an actual `close` option wins over `keep-alive`.
- **A Keep-Alive response still says close:** inspect which `serialize_response` overload the test calls. Exercise the policy-aware overload explicitly while keeping the legacy one-argument overload close-after-response.
- **The wire contains duplicate policy headers or the wrong version:** inspect caller-provided headers and the selected response-version overload. Recheck one authoritative Connection header, computed body-byte Content-Length and the supported HTTP/1.0 or HTTP/1.1 status line.

## Acceptance

The dedicated fixed fixture compiles and tests both maintained protocol copies:

- `labs/w8/http-protocol/reference`
- `labs/w8/router/reference`, which is the protocol copy currently linked by the HTTP server

Each copy must pass the same 14 tests covering HTTP/1.1 default persistence, HTTP/1.0 opt-in, close precedence, token parsing, case handling, whitespace, serializer header uniqueness, computed Content-Length, and backward compatibility.

Run locally from the repository root:

```bash
cmake -S course/fixtures/w8/http-connection-policy -B build-http-connection-policy \
  -DCPPAPPLAB_HTTP_PROTOCOL_SOURCE_DIR="$PWD/labs/w8/http-protocol/reference" \
  -DCPPAPPLAB_ROUTER_SOURCE_DIR="$PWD/labs/w8/router/reference" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http-connection-policy --parallel
ctest --test-dir build-http-connection-policy --output-on-failure
```

## Explain your design

1. Why is HTTP/1.1 persistent by default while HTTP/1.0 is not?
2. Why must Connection options be parsed as comma-separated tokens instead of searched as substrings?
3. Why does close take precedence when both close and keep-alive appear?
4. Why should the serializer own Connection and Content-Length rather than trusting handler-provided values?
5. Why does adding a Keep-Alive response header not by itself make a server safe to reuse a socket?

## Learner task — `w8.http-connection-policy`

**Workspace files**

- `include/http_message.hpp`
- `src/http_message.cpp`

Implement the `ConnectionPolicy` enum, `request_connection_policy(const HttpRequest&)`, and the two policy-aware `serialize_response` overloads described above. Keep the existing one-argument serializer compatible with W8-D5.

**Fixed acceptance:** 14 platform-owned GoogleTests in `course/fixtures/w8/http-connection-policy`. They exercise defaults, HTTP/1.0 opt-in, exact token matching, case/whitespace handling, close precedence, authoritative headers and backward compatibility.

Start after `w8.http-server` has a current PASS. The task workspace starts from the D5 protocol baseline; the D6-A reference copy is not staged into learner work.

```bash
./build/cppapplab start w8.http-connection-policy "$HOME/cppapplab-work/w8-connection-policy"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-connection-policy"
./build/cppapplab hint "$HOME/cppapplab-work/w8-connection-policy" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-connection-policy"
```
