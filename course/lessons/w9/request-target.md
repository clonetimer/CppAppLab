# W9-A2 · Strict HTTP Request-Target and Query Parsing

## Goal

Parse the HTTP request-target once, into a path and an ordered query-parameter list. This stage establishes one canonical interpretation for routing and the later secure static-file task.

## Accepted request-target forms

- Origin-form: /path/to/resource?key=value
- Asterisk-form: OPTIONS *

This educational server does not act as a forward proxy, so absolute-form targets such as http://host/path are rejected. CONNECT authority-form is also rejected.

## Decoding and normalization policy

- The raw target must be visible ASCII, no longer than 8 KiB, and contain no fragment, raw whitespace, control byte, non-ASCII byte or backslash.
- The path must begin with slash and be no longer than 4 KiB.
- Percent escapes are validated and decoded exactly once.
- Percent-encoded slash and backslash in the path are rejected. Literal slash remains the segment separator.
- NUL, control bytes and DEL are rejected after decoding.
- Dot and dot-dot path segments are rejected after the one decoding pass. The parser does not silently normalize them.
- Query strings are limited to 4 KiB, 128 parameters and 1 KiB per decoded name/value.
- Query parameters preserve input order and duplicate names. Empty values and missing equals signs are allowed; empty names and empty parameter segments are rejected.
- Plus remains a literal plus. This parser implements URI query semantics, not application/x-www-form-urlencoded semantics.
- Query percent decoding allows encoded slash; path separator rules do not apply to query values.

The returned path is already percent-decoded. Downstream routing and filesystem code must not decode it again.

## Learner task — w9.request-target

**Workspace files**

- include/http_request_target.hpp
- src/http_request_target.cpp

Implement the parser API, preserve repeated query keys and enforce all limits and rejection rules.

**Fixed acceptance:** 39 platform-owned GoogleTests in course/fixtures/w9/request-target. They cover origin/asterisk forms, query ordering, percent decoding, double-encoding behavior, traversal segments, separators, control bytes and resource limits.

\`\`\`bash
./build/cppapplab start w9.request-target "$HOME/cppapplab-work/w9-request-target"
./build/cppapplab lesson "$HOME/cppapplab-work/w9-request-target"
./build/cppapplab hint "$HOME/cppapplab-work/w9-request-target" 1
./build/cppapplab check "$HOME/cppapplab-work/w9-request-target"
\`\`\`

## Explain your design

1. Why are absolute-form and authority-form rejected by this origin server?
2. Why is percent decoding performed exactly once?
3. Why reject encoded path separators but allow encoded slash in query values?
4. Why reject dot segments instead of silently normalizing them?
5. Why preserve duplicate query keys and their order?
6. Why does plus remain plus in a URI query parser?
7. Why is asterisks-form restricted to OPTIONS?
8. Why should downstream filesystem code consume the decoded path without decoding it again?

## Acceptance

- [ ] Origin-form and OPTIONS asterisk-form are handled.
- [ ] Unsupported request-target forms are rejected.
- [ ] Percent escapes are strict and decoded once.
- [ ] Path separators and dot-segment traversal are rejected safely.
- [ ] Query order and duplicate names are preserved.
- [ ] Query limits and component limits are enforced.
- [ ] Plus and equals semantics are preserved.
- [ ] All 39 fixed checks pass.
