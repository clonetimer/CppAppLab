# Project Status

## Current delivery boundary

W1–W7 and W8-D1–D6-D3 are merged into `main`. The platform includes a real Linux TCP HTTP server with incremental parsing, Router/static-file handling, epoll Reactor, bounded W6 ThreadPool, sequential Keep-Alive, absolute connection deadlines, persistent-connection quotas, bounded graceful shutdown, structured JSONL logs, thread-safe runtime metrics, and fixed operational load/soak regression coverage.

**D5 functional merge:** `f503d3f52a9dcaa9dc5a3df1df424e2b7e65e932`.  
**D5 final-head CI:** run `42956321926`, all three jobs passed.  
**D6-A merge:** `241059c60d3e0ea96c0b3658af1184acc539ab08` (PR #20).  
**D6-B merge:** `8597a38654dbdcebe2d406657d8862f89c0f2788` (PR #21).  
**D6-B final-head CI:** run `42971917343`, all three jobs passed.  
**D6-C merge:** `974b3d211412ebbdc54143d4a5a025a822ce3c85` (PR #22).  
**D6-C final-head CI:** run `42977367404`, all three jobs passed.  
**D6-D1 merge:** `d29d38ec04d4d5a0765f4585c9c25d9df69ab8c2` (PR #23).  
**D6-D1 final-head CI:** run `42979170395`, all three jobs passed.  
**D6-D2 merge:** `546f87c669d7cbf88429e87ed1bc7041313dfc63` (PR #24).  
**D6-D2 final-head CI:** run `42986255957`, all three jobs passed.  
**D6-D3 merge:** `597a8a00a3052b70bf1af7f695c63f7310d67563` (PR #25).  
**D6-D3 final-head:** `736a0bac1e859c8cdffbfe7b1883e709cb928eec`; CI run `42991542424`, all three jobs passed.  
**Advanced taskization merge:** PR #26 merged into `main` as `c8f8e03006e691db41f2a58664d2fe1e14afde78`. Final PR-head CI run `42998617546` passed all three jobs. Root suite 60/60; six task-mode fixtures 49/49 under GCC, ASan/UBSan and clang-tidy.  
**Target-machine status:** Windows 11 + WSL2 acceptance has not yet been performed or claimed. No Release is declared.

## Verified merged baseline

- W1–W7 learning engine and task packs are merged into `main`.
- W8-D1 HTTP protocol foundations: 33 fixed acceptance checks.
- W8-D2 incremental parser and strict HTTP header field validation: 39 checks.
- W8-D3 responses and static files: 31 checks.
- W8-D4 Router and dynamic handlers: 43 checks.
- W8-D5 TCP/epoll + worker-pool HTTP server: 34 fixed checks (23 platform-owned + 10 reference learner tests + one real loopback CLI smoke).
- W8-D6-A connection policy: 14 fixed tests against each maintained protocol copy (28 executions).
- W8-D6-B sequential Keep-Alive: eight fixed loopback tests for sequential reuse, pipelining, explicit close, fragmented requests, HTTP/1.0, half-close draining and response ordering.
- W8-D6-C connection guards: nine fixed loopback tests for idle/header/body deadlines, absolute trickle-resistant deadlines, idle refresh, request-count cap, persistent-connection quota and invalid configuration. Final-head run `42977367404` passed root GCC/CTest 53/53, root ASan/UBSan 53/53, D6-C fixture 9/9 under GCC and sanitizer, D6-B fixture 8/8 under GCC and sanitizer, D6-A fixture 28/28 under GCC and sanitizer, formatting and clang-tidy across all direct fixtures.
- W8-D6-D1 graceful shutdown: five fixed loopback tests for idle connection close, in-flight response drain, incomplete-request 503, shutdown deadline and invalid configuration. Final-head run `42979170395` passed all three jobs; D6-D1 fixture 5/5 under GCC, ASan/UBSan and clang-tidy.
- W8-D6-D2 observability: eight fixed loopback tests for structured access logs, query redaction, connection gauges, handler failures, log-sink exception isolation, timeout counters, connection rejection and worker-queue rejection. Final-head run `42986255957` passed all three jobs; root GCC/CTest 53/53, root ASan/UBSan 53/53, HTTP Server reference workspace 10/10, D6-A 28/28, D6-B 8/8, D6-C 9/9, D6-D1 5/5 and D6-D2 8/8. Formatting and clang-tidy passed for all configured direct fixtures.

## W8-D6-D3 — operational load and soak validation (merged)

**Merge:** PR #25, commit `597a8a00a3052b70bf1af7f695c63f7310d67563`.  
**Final tested head:** `736a0bac1e859c8cdffbfe7b1883e709cb928eec`.  
**Final CI:** run `42991542424`, all three jobs passed.

The four-case loopback fixture in `course/fixtures/w8/http-operational` verifies:

1. Sixteen concurrent clients; handler concurrency remains within the four-worker bound and all responses complete.
2. An 8 MiB response to a deliberately slow reader; non-blocking backpressure, EPOLLOUT recovery and complete body delivery.
3. Eight concurrent Keep-Alive clients × forty requests (320 total); no queue rejection, protocol errors or leaked connections/jobs.
4. Eight repeated server construction/run/stop/join/destruction cycles with idempotent stop requests.

Final-head regression evidence: root GCC/CTest 53/53; root ASan/UBSan 53/53; HTTP Server reference workspace 10/10; D6-A 28/28; D6-B 8/8; D6-C 9/9; D6-D1 5/5; D6-D2 8/8; and D6-D3 4/4 under GCC, ASan/UBSan and clang-tidy. Formatting and clang-tidy passed across the configured direct fixtures.

These are deterministic functional load/soak regressions and resource-recovery checks, not throughput, tail-latency or production-capacity SLAs.

## W8 advanced tutorial taskization (merged into main via PR #26)

**Branch:** `feat/w8-advanced-taskization`.

The catalog grows from 14 to 20 tasks. Six sequential learner tasks are registered after `w8.http-server`, with dedicated learner templates, three progressive hints, and fixed acceptance fixtures that compile the learner's declared source files:

| Task | Focus | Fixed checks |
|---|---|---:|
| `w8.http-connection-policy` | HTTP/1.0/1.1 persistence and authoritative response serialization | 14 |
| `w8.http-keep-alive` | Sequential reuse, pipelining and half-close draining | 8 |
| `w8.http-connection-guards` | timerfd deadlines and connection quotas | 9 |
| `w8.http-graceful-shutdown` | In-flight drain and bounded shutdown | 5 |
| `w8.http-observability` | JSONL logs, redaction and atomic metrics | 8 |
| `w8.http-operational-load` | Concurrent Keep-Alive load probe and latency statistics | 5 |

The tasks form a strict prerequisite chain from D6-A through D6-D3. D6-B through D6-D2 use a known completed previous-stage baseline so each task concentrates on its own implementation delta; the learner still implements the stage under test. D6-D3 adds a reusable load-probe module while the existing four-case operational regression fixture remains an independent server-level gate.

Task-mode CI configures each fixed fixture with `CPPAPPLAB_LEARNER_SOURCE_DIR` pointing to the corresponding reference implementation. This validates the same CMake path used by `cppapplab check` without pre-filling learner workspaces. Final PR-head run `42998617546` passed the learner-mode 49/49 checks under GCC, ASan/UBSan and clang-tidy. The root workflow test also created each learner workspace, copied its known-good stage implementation, invoked `cppapplab check`, and verified a current PASS for all six tasks.

## W9-A1 — HTTP/1.1 chunked transfer decoding (merged and validated)

**Merge:** PR #27, commit `bfb3f3f87b526bb4c65adaae19bdacceda76acfe`.  
**Task ID:** `w9.http-chunked`.  
**Prerequisite:** current PASS for `w8.http-operational-load`.

The merged main catalog now contains 21 tasks. The learner source boundary is the W8 HTTP message model and incremental request parser. The learner template retains the working W8 Content-Length parser and marks the Transfer-Encoding branch as the implementation TODO.

The W9-A1 reference parser implements:

- HTTP/1.1-only, single-coding chunked request framing.
- Rejection of Content-Length + Transfer-Encoding ambiguity and unsupported coding chains.
- Incremental chunk-size, payload, CRLF and trailer parsing.
- Binary-safe body assembly and exact consumed-byte reporting for pipelined requests.
- 1 MiB decoded-body, 1 KiB chunk-size-line and 16 KiB trailer-section limits.
- Trailer syntax validation, duplicate rejection and forbidden framing/routing field rejection.

The fixed fixture contains 35 platform-owned GoogleTests. A root learning-engine workflow test creates the W9 workspace, copies the known-good reference source, replaces learner CMake with a fatal error and invokes the real `cppapplab check` path. Final PR-head run `43006252261` passed all three jobs: root 62/62 and W9 fixture 35/35 under GCC, ASan/UBSan and clang-tidy. The end-to-end W9 learning check also passed.

W9-A1 is merged and validated. W9-A2 is separately merged and validated as recorded below.
 
## W9-A2 — Strict request-target and query parsing (merged and validated)

**Merge:** PR #28, commit `cca1c808499761d01a638f5d55c30452c15e5296`.  
**Task ID:** `w9.request-target`.  
**Prerequisite:** current PASS for `w9.http-chunked`.

The task introduces a standalone request-target parser. It accepts origin-form and OPTIONS asterisk-form, rejects absolute/authority forms for this origin server, decodes percent escapes exactly once, preserves ordered duplicate query parameters, treats plus as a literal plus, rejects fragments/control bytes/dot segments/encoded path separators, and applies explicit target/path/query/component limits.

The learner source boundary is a new request-target header/source pair. The reference fixture contains 39 platform-owned GoogleTests. A root workflow test creates the learner workspace, copies only the declared reference source, disables learner CMake and invokes the real `cppapplab check` path. Final PR-head run `43008862704` passed all three jobs: root 64/64 and W9-A2 39/39 under GCC, ASan/UBSan and clang-tidy; W9-A1 remained 35/35. End-to-end learning-engine checks verified current PASS evidence for both W9 tasks.

## W9-A3 — Secure static-file root confinement (merged and validated)

**Merge:** PR #29, commit `f741e3440ac9e3dbf206b8f19215ca3b18720b71`.  
**Final tested head:** `1018190a93bb1bdd134d0d347c71e9497f39485e`.  
**Final PR CI:** run `43013017702`, all three jobs passed.  
**Task ID:** `w9.static-root`.  
**Prerequisite:** current PASS for `w9.request-target`.

The main catalog now contains 23 tasks. The learner workspace stages the completed W8 HTTP message model and W9-A2 request-target parser as known-good baselines. The learner implements the static-file handler.

The reference handler:

- Opens the document root once with `O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW` and retains its descriptor.
- Traverses intermediate components with `openat`, `O_DIRECTORY` and `O_NOFOLLOW`.
- Opens the final component with `O_NOFOLLOW | O_NONBLOCK`, then requires `S_ISREG` from `fstat`.
- Uses the W9-A2 parser exactly once; no second percent-decoding pass occurs.
- Maps root/trailing-slash paths to `index.html`, preserves binary bytes and MIME types, and enforces a maximum read size before and during the read.
- Rejects final and intermediate symlinks, FIFOs, directories and path traversal.

Final acceptance evidence:

- Root learning-engine suite: 66/66 under GCC, ASan/UBSan and clang-tidy.
- W9-A3 fixed fixture: 34/34 under GCC, ASan/UBSan and clang-tidy.
- W9-A1 regression: 35/35; W9-A2 regression: 39/39.
- End-to-end `cppapplab check` created the W9-A3 learner workspace and verified current PASS evidence.
- clang-format and clang-tidy passed.

All symlinks below the configured document root are rejected, including symlinks that point back inside the root. The configured root itself is trusted configuration; its final component must not be a symlink. This is an educational security boundary, not a claim of full production filesystem sandboxing.

## Usability and first-use experience

The platform now has a guided learner entry point (`cppapplab learn`) and a one-command Ubuntu setup path (`bash scripts/quickstart.sh`). The guided command selects the next unlocked task, resumes a saved workspace or creates a non-overwriting default workspace, displays the task guide and prints the exact next commands. The quick-start script checks the Ubuntu 24.04 toolchain, requests approval before installing missing packages, builds the project and runs CTest before entering the first task. These changes reduce first-use command and workspace-management overhead; they do not replace target-machine acceptance.

## Interactive learner experience and feedback

The CLI now opens a terminal home screen when `cppapplab` is run without arguments in an interactive terminal. Learners can continue, choose tasks by number, or review progress without memorizing task IDs. Failed acceptance checks show a concise compiler/test diagnostic excerpt and direct hint, log and retry commands. Non-interactive invocations retain help output to avoid blocking scripts and CI.

## Remaining work

1. Complete Windows 11 + WSL2 target acceptance.
2. Finish reproducible/offline dependency delivery and release qualification.

W9-A1 chunked-transfer decoding, W9-A2 request-target/query parsing and W9-A3 secure static-root confinement are merged and validated. W9-A4 HTTP hardening and W9-A5 parser fuzzing remain planned; TLS/WebSocket, performance engineering and production operations are planned for W10–W12. These later stages are not complete or release-qualified.

A repository commit is not passing evidence. Record exact tested SHA, workflow result, test counts and target environment. CI does not certify human understanding or target-machine compatibility.
