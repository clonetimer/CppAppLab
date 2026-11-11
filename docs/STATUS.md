# Project Status

## Current delivery boundary

The project provides a C++17 learning engine and cumulative W1–W8 task packs. W8-D1–D4 provide the HTTP parser, static-file handler, Router, dynamic handlers and serializer. W8-D5 is being implemented on `feat/p5-w8-http-server`: a real TCP listener, epoll Reactor and bounded W6 ThreadPool handoff. Until D5 is merged and validated, `main` still contains only the stdin-to-HTTP-response Router CLI.

No Release or target-machine Windows 11 + WSL2 acceptance is declared. Hosted Ubuntu validation and local target acceptance are separate.

## Verified merged baseline

- W1–W7 learning-engine and task history is preserved in `archive/STATUS-before-w8-router.md` and Git history. That archive is historical and includes superseded status labels; do not treat it as the current status source.
- W8-D1 HTTP protocol foundations: PR #12; catalog acceptance 33.
- W8-D2 incremental request parser: PR #13, then strict field validation in PR #14; catalog acceptance 39. Retain the merged hardening rather than the old 37-check contract.
- W8-D3 responses and static files: PR #16 merged as `f35ffcb7966d0bb002b5ab08c5f21efa804d1332`. Its final tested head was `1be123945704b0b5cc2ebf22389f7528075c95ed`; hosted run `42731934255` passed GCC, ASan/UBSan and formatting/clang-tidy jobs. Fixed acceptance remains 31 (20 platform + 10 reference-as-learner + CLI).

## W8-D4 Router and dynamic handlers

**Branch:** `feat/p5-w8-router`. Implementation and fixed acceptance are included in this change; the associated PR and final-head Actions results are the authority for integration status.

- Method + exact path lookup; query stripping for matching without altering the request passed to the callback.
- GET /hello, POST /echo and GET /health, with the outputs specified by the supplied course.
- Known dynamic paths return 405 with sorted Allow on method mismatch. A callback's 404 does not cause static fallback.
- Existing static handling remains the fallback only for absent dynamic paths.
- Invalid registration and empty handlers are rejected before replacing an existing entry.
- 32 fixed GoogleTest cases, ten learner tests and one byte-exact CLI integration smoke: 43 checks. Fourteen declared source files participate in the workspace fingerprint.
- Root workflow checks the real `cppapplab check` path and deliberately disables learner CMake to preserve the fixed-fixture boundary.

Before hosted integration, local native C++ probes observed Router 0/14 → 14/14 and API/dispatch 0/12 → 12/12, followed by successful direct ASan/UBSan runs. The real parser/dispatch/serializer CLI smoke was observed failing against an empty CLI and passing against the implementation. These probes are not a substitute for the project's full GoogleTest/CTest suite, which runs in hosted CI.

The CI now also compiles and tests the Router fixed fixture directly with ASan/UBSan and clang-tidy. Root build options do not automatically instrument independently configured nested acceptance projects; a green root sanitizer job must not be described as blanket sanitizer coverage of every lab implementation.

## W8-D5 HTTP TCP Server — in progress

**Branch:** `feat/p5-w8-http-server`  
**Base:** `main` at `dcee35395349104aad31ef1d2f1ad645e4c9fe06`  
**Status:** design, API, reference implementation, learner scaffold and fixed loopback acceptance fixture are being assembled; hosted CI has not yet run.

- Single Reactor owns listener/client sockets, epoll registrations, connection maps and read/write buffers.
- Workers receive owned HttpRequest values and publish HttpResponse completions; they never touch sockets or Connection objects.
- Generation tokens identify connections and prevent stale completions from being applied after fd reuse.
- eventfd wakes the event loop after worker completion.
- W6 ThreadPool is reused unchanged. Since its submit queue blocks, an event-loop-owned outstanding_jobs counter includes queued, running and undrained completions. The server returns 503 before submit at task_queue_capacity, including after disconnected clients, so the event loop never waits for queue space.
- D5 preserves the existing serializer's Connection: close behavior: one request per TCP connection. Keep-Alive/pipelining is deferred to D6.
- Fixed acceptance is planned as 19 platform-owned tests + 10 reference tests + one real loopback CLI smoke (30 total).
- No merge, hosted CI PASS, release, or local WSL2 acceptance is claimed yet.

## Source and scope

The implementation follows the supplied `快速C++` D4 section. The optional hasPath/405 discussion is adopted consistently, and registration validation, nested-key storage and deterministic Allow are identified as engineering refinements in the lesson. Access to `clonetimer/cpptest` returned 404; its code has not been claimed as incorporated.

Known boundaries remain: no chunked Transfer-Encoding, no query parameter parser or URL decoding, no route patterns, and no production canonical/symlink containment for the carried-forward D3 static handler. Register routes before dispatch; concurrent registration is not supported.

## Remaining work

1. Finish W8-D5 code review, hosted GCC/ASan/UBSan/clang-tidy and merge only after final-head CI passes.
2. W8-D6: HTTP/1.1 Keep-Alive, deadlines, request limits, logging and graceful shutdown policies.
3. Full course workflow, reproducible/offline dependency delivery, local Windows/WSL2 acceptance and release qualification.

A repository commit is not passing evidence. Record the final tested SHA, workflow result and target environment before claiming completion; learner progress files remain editable navigation data, not tamper-proof certificates.
