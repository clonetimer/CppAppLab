# Project Status

**Current milestone:** P5-C — W8 Static File Handler and HTTP Responses  
**Release status:** no release has been declared.

## Merged P0 foundation

P0 PR #1 is merged into `main`.

- C++17 CMake project and minimal CLI
- GoogleTest / CTest baseline
- GCC warning and optional ASan/UBSan settings
- clang-format and clang-tidy integration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, roadmap, and decision documents
- eight-week curriculum map

## Merged P1-A — W1 Hello World

P1-A PR #2 is merged into `main`.

- task registry and CLI: `list/start/lesson/hint/check/status/next`
- learner workspace creation with no-overwrite behavior
- separate learner and reference implementations
- fixed GCC compile/run check with exact-output acceptance
- bounded process output and execution time
- process-group termination on timeout
- source fingerprint and stale local evidence detection
- symlink/path-traversal rejection for declared learner source paths

Hosted Ubuntu 24.04 CI passed 15/15 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

## Merged P1-B — W1 multi-file Calculator

P1-B PR #3 is merged into `main`.

- second registered task: `w1.calculator`
- multi-file learner project and separate reference implementation
- platform-owned CMake/GoogleTest fixture
- seven fixed unit tests plus two CLI smoke checks
- learner CMakeLists.txt and tests are not the acceptance authority

Hosted Ubuntu 24.04 CI passed 17/17 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

## P1-C — persistent progress and workspace recovery

**Branch:** `feat/p1c-progress-state`  
**Status:** merged into `main`; PR #4 is closed.

- task catalog externalized to `course/tasks.tsv`, with validation for paths, acceptance metadata, duplicate IDs, and prerequisite ordering
- explicit prerequisite metadata; Calculator is locked until Hello World has a current PASS
- course progress persisted outside the source checkout
- `progress` displays NOT STARTED, IN PROGRESS, LOCKED, PASSED, NEEDS WORK, and STALE states
- `resume <task-id>` restores the latest saved workspace, including before the first check
- `history <task-id>` displays previous check attempts
- every check records the latest result and appends a bounded history record
- prerequisite PASS can be satisfied by any workspace whose latest attempt is PASS and whose current source fingerprint still matches
- starting a second workspace does not erase a valid PASS from another workspace

Default state location:

`~/.local/share/cppapplab/progress/`

Override with `CPPAPPLAB_STATE_DIR`.

Hosted Ubuntu 24.04 verification on the P1-C branch passed 28/28 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks. PR #4 was merged as `ff137ea16fed8293dca6ca7e8972d83c72a265e4`. The environment checker reported zero failures and one expected WSL2 warning on the GitHub-hosted runner.

## P2-A — W2 persistent Todo CLI

**Branch:** `feat/p2-w2-todo-cli`  
**Status:** merged into `main` via PR #5.

- task `w2.todo-cli`, dependent on a current W1 Calculator PASS
- STL-backed Todo model using `std::vector`, algorithms, and `std::unordered_map`
- add/list/done/remove command-line operations
- versioned text persistence with escaped titles
- transactional load behavior and atomic save publication
- platform-owned interface and 18 unit tests plus one multi-process CLI workflow test

Hosted Ubuntu 24.04 CI on commit `6e7b3f3b9f5c68f24e832afe33af3e9d0727ad5f` passed 30/30 root tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks. The W2 end-to-end acceptance test independently ran the 19-case Todo fixture and passed.

## P2-B — W3 Task / TaskManager Refactor

**Branch:** `feat/p2-w3-task-manager`  
**Status:** merged into `main` via PR #6.

- `Task` value object with constructor invariants and ID-based equality/order
- `TaskManager` owns tasks through `std::unique_ptr<Task>`
- deep-copy constructor, copy-and-swap assignment, move construction/assignment
- non-owning lookup and ordered value snapshots
- W2-compatible `CPPAPPLAB_TODO_V1` persistence with transactional load and atomic save
- unchanged add/list/done/remove CLI behavior
- platform-owned acceptance fixture: 30 GoogleTest cases plus one CLI workflow test (31 total)
- W3 depends on a current W2 Todo CLI PASS

Hosted Ubuntu 24.04 CI on commit `01325e756b70f951bda2dfc58ed6d48acbad80fd` passed 32/32 root tests in ordinary GCC and ASan/UBSan builds. The W3 platform-owned fixture passed 31/31; clang-format and clang-tidy-enabled checks passed. The environment checker reported zero failures and one expected WSL2 warning on the GitHub-hosted runner.

## P3-B — W5 Linux System Programming Lab

**Branch:** `feat/p3-w5-linux-systems`  
**Status:** merged into `main` via PR #8.

- bounded thread-safe producer/consumer queue with close/drain semantics
- POSIX descriptor write helper and child process runner using pipe/fork/dup2/execvp/waitpid
- bounded output capture, exit/signal status and timeout kill/reap
- 21 platform-owned tests, 10 learner tests and one CLI smoke test (32 total)
- W5 depends on a current W4 quality-gate PASS

Hosted Ubuntu 24.04 CI on final PR head `88d43e8425467e5f4b7cb0d181c625aa66199475` passed: GCC root tests 36/36, ASan/UBSan 36/36, W5 fixed fixture 31/31, CLI smoke, environment acceptance, clang-format and clang-tidy-enabled build/tests.

## P3-C — W6 Thread Pool and Futures

**Branch:** `feat/p3-w6-thread-pool`  
**Status:** merged into `main` via PR #9.

- fixed-size worker pool built on the W5 bounded queue
- templated submit with move-aware callable/argument storage and std::future results
- task exceptions transported through futures
- lifecycle mutex serializes submit acceptance with shutdown
- std::call_once ensures concurrent shutdown callers wait for worker joins
- shutdown drains accepted work and rejects later submissions
- 23 platform-owned tests, 10 learner tests and one CLI smoke test (34 total)
- W6 depends on a current W5 system-lab PASS

Hosted Ubuntu 24.04 CI on final PR head `0b9a453c1ab21e62ad81723ecb4618bce17a045f` passed: GCC build and tests, ASan/UBSan, clang-format, clang-tidy-enabled build/tests, CLI smoke, environment acceptance, and the 34-case W6 fixed fixture. Local Windows 11 + WSL2 acceptance remains separate.

## P4-A — W7 TCP Echo Server and Client

**Branch:** `feat/p4-w7-tcp-networking`  
**Status:** merged into `main` via PR #10.

- IPv4 TCP listener and client using Linux socket APIs
- blocking accept/recv/echo loop and complete send handling
- RAII descriptor ownership and EINTR handling
- learner/reference separation and platform-owned acceptance fixture
- 20 platform-owned tests, 10 learner tests and one CLI smoke test (31 total)
- W7-A depends on a current W6 Thread Pool PASS

Hosted Ubuntu 24.04 CI on final PR head `c20c60665d891edd54106779f99e84fbc3f58127` passed: GCC build/tests, ASan/UBSan, clang-format, clang-tidy-enabled build/tests, CLI smoke and environment acceptance. The W7-A fixture passed 21/21 platform tests, 10/10 learner tests and the CLI smoke test. Local Windows 11 + WSL2 acceptance remains separate.

## P4-B — W7 Multi-client TCP Chat Server

**Branch:** `feat/p4-w7-epoll-chat`  
**Status:** next development stage.

- thread-per-connection server to expose connection lifecycle and ownership
- non-blocking sockets and EAGAIN/EWOULDBLOCK handling
- select/poll progression, then epoll event loop
- per-client input framing and output buffering for partial I/O
- final multi-client chat server with bounded input, disconnect handling and tests

## P4-B — W7 Multi-client TCP Chat Server

**Branch:** `feat/p4-w7-epoll-chat`  
**Status:** merged into `main` via PR #11; hosted Ubuntu 24.04 CI passed.

- Linux non-blocking IPv4 listener and level-triggered epoll event loop
- single-thread ownership of client state
- nickname registration and newline-delimited chat commands
- fragmented/coalesced input parsing and per-client output buffering
- bounded client count, line length and pending output
- eventfd stop wakeup and deterministic event-loop cleanup
- 26 platform-owned tests, 10 learner tests and one CLI smoke test (37 total)
- W7-B depends on a current W7-A TCP Echo PASS

## P5-A — W8 HTTP Protocol Foundations

**Branch:** `feat/p5-w8-http-protocol`  
**Status:** W8-D1 merged into `main` via PR #12; hosted Ubuntu 24.04 CI passed.

- HTTP/1.1 request and response message structure
- request-line parsing: method, target and version
- response serialization with byte-accurate Content-Length
- status line, headers and CRLF framing
- W8-D1 protocol foundation precedes the incremental request parser in W8-D2
- 22 platform-owned tests, 10 learner tests and one CLI smoke test (33 total)
- W8-D1 depends on a current W7-B TCP chat PASS

## P5-B — W8-D2 Incremental HTTP Request Parser

**Base implementation:** merged into `main` via PR #13; hosted Ubuntu 24.04 CI passed.  
**Follow-up:** strict HTTP request-line and header-field validation on `feat/p5-w8-http-parser-hardening`.

- incremental request parser over arbitrary TCP byte chunks
- HTTP method token, request-target and version validation
- case-insensitive header names stored lowercase
- rejects whitespace before the header colon, invalid field-name tokens and control bytes in values
- header terminator detection using CRLFCRLF
- Content-Length body accumulation by bytes
- NeedMoreData / Complete / Error parser states and pipelined consumed-byte reporting
- duplicate headers rejected as an explicit learning policy; Transfer-Encoding remains unsupported
- parser limits: 16 KiB headers and 1 MiB bodies
- 28 platform-owned tests, 10 learner tests and one CLI smoke test (39 total)

## P5-C — W8-D3 HTTP Responses and Static Files

**Branch:** `feat/p5-w8-static-files`  
**Status:** next implementation stage.

- response serialization integrated with request handling
- binary static-file reads from a document root
- MIME type selection for html/css/js/txt/json/png/jpeg/svg
- GET-only static handler with 405 and Allow: GET
- 404 for missing resources; 400 for unsafe target paths
- query string removal before file lookup
- conservative path traversal guard; production-grade canonical/symlink containment remains out of scope

## Remaining work

- additional W1–W8 learner/reference task packs
- richer task-specific diagnostics and feedback
- verified offline dependency package
- local Windows 11 + WSL2 + Ubuntu 24.04 acceptance
- first release package and release notes

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. Local progress and evidence files are editable navigation data, not tamper-proof certificates. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
