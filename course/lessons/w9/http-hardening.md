# W9 · A4 HTTP Request Admission Policy

## Start here

1. Read `include/http_hardening.hpp` and the A1 parser interface.
2. Implement only `src/http_hardening.cpp`. The platform supplies the completed A1 parser and the fixed admission interface.
3. Start with a complete valid request, then an incomplete one. Return a request only on Accepted.
4. Add header count/byte limits, decoded body and per-request wire limits. Use the fixed tests to distinguish malformed input from resource rejection.
5. Build, run tests and formally check. Explain which bytes remain for the next request.

## Goal

Add an explicit, bounded, deterministic admission policy over the already-tested W9-A1 parser. Do not duplicate parser grammar or edit the previous course. All examples are in-memory fixtures; do not send test inputs to external services. The exercise is a local strict service policy, not a claim of exhaustive HTTP compliance or production security.

## Your task

Edit only `src/http_hardening.cpp`. Read-only headers: `include/http_hardening.hpp`, `include/http_request_parser.hpp`, and `include/http_message.hpp`.

| Contract | Limit / behavior |
|---|---|
| Header | at most 8192 wire bytes including blank-line terminator; at most 32 fields, excluding request line |
| Decoded body | 65536 bytes; reject larger declared Content-Length as soon as complete header is parsed |
| Per-request wire | at most 98304 bytes; inspect a bounded prefix, not the entire receive buffer |
| NeedMoreData | consumed = 0, empty request, no mandatory close |
| Accepted | exact consumed bytes of the first message, full request, no mandatory policy close |
| BadRequest / HeaderTooLarge / PayloadTooLarge | consumed = 0, empty request, mandatory close |

Budget precedence: first header byte/count budget; then A1 parser grammar and framing; then nonempty Host for HTTP/1.1 and declared body budget; then incomplete wire budget or completed decoded body budget. Map A1's header/trailer-size errors to HeaderTooLarge and body-size error to PayloadTooLarge; other parser errors are BadRequest. An incomplete header at its byte limit cannot complete within policy. A completed first message at or below the limit may be followed by any suffix; the suffix must not change admission. A chunked body is bounded by wire budget while incomplete and checked against decoded-body budget when complete.

Do not expose partially parsed request data or consume bytes on failure. close_connection means the policy requires closing on rejection; full server connection negotiation is outside this lab. An incomplete or accepted request does not by itself grant keep-alive.

## Build and run

Use the workbench Build, Run / Test and Formal Acceptance controls; the fixed fixture uses C++17, GCC, CMake and GoogleTest. Its 24 cases test real parser behavior. The learner header and local build file cannot replace the fixed contract.

## Explain your design

Why is a per-request cap different from a total buffer cap? Why should malformed framing fail closed? Why must an incomplete result hide partial headers and body? What must an actual server additionally do with deadlines, concurrent memory and connection closure?

## Troubleshooting

- A valid request is rejected with a large suffix: bound the first parse window, not the total buffer size.
- Partial data is accepted or request fields leak: return a default incomplete result, not the partial parser request.
- Resource rejection becomes generic malformed input: distinguish the documented admission statuses.
- A large Content-Length waits indefinitely: check the declared size after a complete valid header, before waiting for the body.
- A body is decoded but exceeds the local policy: check decoded length before handing it to the application.

## Acceptance

- [ ] Fixed C++17 source compiles against platform-owned A1 dependency and admission interface.
- [ ] All 24 fixed tests pass, including normal/partial, strict framing, header, field, body, wire and suffix boundaries.
- [ ] No failure consumes bytes or exposes partial request state; errors require closure.
- [ ] Existing W9-A1–A3 contracts remain unchanged.
- [ ] Explain the local policy boundary; no claim of a deployed hardened server.
