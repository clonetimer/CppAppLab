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

The platform now has a guided learner entry point (`cppapplab learn`) and a one-command Ubuntu setup path (`bash scripts/quickstart.sh`). The guided command selects the next unlocked task, resumes a saved workspace or creates a non-overwriting default workspace, displays the task guide and prints the exact next commands. The quick-start script checks the Ubuntu 22.04 or 24.04 toolchain, requests approval before installing missing packages, builds the project and runs CTest before entering the first task. These changes reduce first-use command and workspace-management overhead; they do not replace target-machine acceptance.

## Interactive learner experience and feedback

The CLI now opens a terminal home screen when `cppapplab` is run without arguments in an interactive terminal. Learners can continue, choose tasks by number, or review progress without memorizing task IDs. Failed acceptance checks show a concise compiler/test diagnostic excerpt and direct hint, log and retry commands. Non-interactive invocations retain help output to avoid blocking scripts and CI.

The interactive task picker now supports number, task-ID and title-keyword selection. Its task list distinguishes PASSED, IN PROGRESS, NEEDS WORK, STALE, AVAILABLE and LOCKED; the home screen summarizes those counts so learners can find unfinished work without remembering identifiers.

**Usability delivery is merged:** PR #30 (`917b45c`) added `cppapplab learn` and the one-command Ubuntu quick start; PR #31 (`89ea327`) added the interactive learner home and actionable check diagnostics; PR #32 (`991007e`) added title-keyword search and granular progress states. Final PR #32 CI run `43024884781` passed all three jobs: root GCC/CTest 75/75, root ASan/UBSan 75/75, clang-format and clang-tidy, plus the W8/W9 acceptance fixtures. This validates the hosted Ubuntu workflow; Windows 11 + WSL2 target-machine acceptance remains separate.

**Windows launcher merged:** PR #33 (986e9ff) adds the double-click Start-CppAppLab.cmd entry point, PowerShell 5.1/7 distribution detection and WSL2 checks, first-run Linux checkout preparation, safe existing-checkout validation and an explicit rebuild mode. Final CI run 43031051999 passed all four jobs: GCC 75/75, ASan/UBSan 75/75, clang-format/clang-tidy and Windows PowerShell syntax; Bash bootstrap tests also covered path expansion, spaces, launch and rejection of unrelated repositories. Actual WSL2 runtime acceptance on a target Windows machine remains separate.

## P7 — Delivery and stabilization

**P6 exit verification: PASS.** Mainline CI run #1183 (43132738098) completed all five jobs successfully on merge commit fe06f4c0ed2d2fb38ca92d5b16117c80939a9ee2: root GCC/CTest 75/75, root ASan/UBSan 75/75, formatting/clang-tidy, Windows PowerShell 7 + 5.1 + WSL command handoff, and source/learner package smoke tests. W8 D6 and W9-A1/A2/A3 fixtures all passed in the GCC, sanitizer and clang-tidy jobs. This is the current P6 exit evidence.

### P7-A — Reproducible source and learner packages (merged)

**PR #35 merged:** fe06f4c0ed2d2fb38ca92d5b16117c80939a9ee2.

- Deterministic source archive from the exact Git commit.
- Relocatable native Ubuntu 22.04 and Ubuntu 24.04 x86_64 learner archives, each with a matching prebuilt CLI, course content, templates, fixtures, reference baselines and first-use documentation.
- Wrapper sets CPPAPPLAB_DATA_DIR to the extracted package root.
- Package manifest records source commit, target, filenames and SHA-256.
- CI extracts the learner package into a different directory, discovers W1/W8/W9 tasks, creates a W1 workspace and passes its fixed Hello World acceptance check.
- CI package artifact is available from [run #1183](https://github.com/clonetimer/CppAppLab/actions/runs/43132738098); artifacts have 14-day retention.

### P7-B — Offline dependency bundle (merged)

PR #36 (`ec5a68f`) merged the offline dependency bundle generation, package integrity and no-install APT simulation workflow. The Ubuntu 22.04 compatibility follow-up extends the same bundle format to Jammy while retaining the original Noble target. [CI #1249](https://github.com/clonetimer/CppAppLab/actions/runs/43149538317) passed Windows and package checks, including 171 verified packages. The repository now retains this optional capability without requiring users to install Ubuntu.

The offline CI job refreshes only APT package indexes from a repository-pinned Ubuntu snapshot; it does not install packages or modify the dpkg database. The snapshot ID is included in the generated package lock and build metadata.

- Required learner/build toolchain: build-essential, CMake, Git, GDB and GoogleTest.
- Optional quality tools: clang-format and clang-tidy, stored as an incremental package group.
- Read-only APT URI resolution using apt-get --print-uris; bundle creation does not run apt-get update or install packages.
- Downloaded .deb closure with package name/version/architecture/dependency/size/SHA-256 lock metadata.
- Installer modes --verify-only and --dry-run; dry-run resolves against a temporary local APT repository and empty dpkg status database, while actual installation uses local .deb files with --no-download.
- Unit tests for APT URI parsing and Windows Git-Bash syntax checks.

The archive was built and checked with dry-run simulation; it was not installed on the user's machine.

### P7-C — Target-machine acceptance and release qualification

- Validate Windows 11 + WSL2 + Ubuntu 22.04 and Ubuntu 24.04 launch with the matching prebuilt learner and offline dependency archives.
- Verify workspace/progress persistence after package replacement.
- Record exact package SHA-256, environment, commands and test evidence.
- Publish version tag, release notes, known limitations and evidence archive only after P7-A/B/C pass.


## Ubuntu 22.04 compatibility follow-up (PR #38)

**Branch:** `feat/ubuntu-22.04-support-20261113`.

- Source builds, quick start, environment checks and the Windows launcher support Ubuntu 22.04 LTS and Ubuntu 24.04 LTS.
- The root GCC/GoogleTest suite passed 78/78 on Ubuntu 22.04.
- PR CI passed the full GCC/GoogleTest matrix on both releases and the Windows launcher syntax/handoff tests.
- Native learner archives are built on the matching Ubuntu release to avoid shipping a newer libstdc++ runtime to Ubuntu 22.04.
- Offline dependency archives resolve from the matching Jammy/Noble archive snapshot and validate their target release before installation.
- ASan/UBSan, clang-tidy and offline dependency packaging are being added to the pull-request matrix; Windows target-machine acceptance remains a separate check.

## Course-first usability focus (2026-11-12)

The priority is a complete, understandable experience for the currently released **23 W1–W9 tasks**, not Ubuntu installation. A new course-focused PR adds a Chinese learning route, an interactive `guide` entry, correct launch commands in extracted learner packages, Markdown repairs and an automated content audit. Planned W9-A4/A5 and W10–W12 lessons remain outside the current registered course. The tutorial acceptance uses existing preinstalled CI toolchains and does not install Ubuntu packages.


## P8 — Browser-first learning workbench

**P8-A through P8-C5 are implemented and merged into `main` via PRs #40–#44.**

- P8-A/P8-B: PR #40, merge `d4cadbf`.
- P8-C1/C2: PR #41, merge `edc8479`.
- P8-C3: PR #42, merge `aaef088`.
- P8-C4: PR #43, merge `ffd25b4`.
- P8-C5 integrated browser course, recovery, evidence automation and beginner-first lesson guidance: PR #44, merge `25686fd210098006ffcadf8891c54f043e0b148d`.
- Final PR CI for P8-C5: [#1848](https://github.com/clonetimer/CppAppLab/actions/runs/44104945290), required jobs passed. The repository-wide formatting step was manual/opt-in and skipped; Ubuntu 22.04 source-build compatibility is push-only and was not exercised by the PR event.
- Merged-main push CI: [#1849](https://github.com/clonetimer/CppAppLab/actions/runs/44119538209), all five scheduled jobs passed, including Ubuntu 22.04 source-build compatibility. Full GCC/sanitizer/quality matrices and offline bundles are PR/manual gates and were skipped on this push event.
- Browser evidence from the accepted PR run includes desktop/mobile screenshots and the generated course coverage CSV. Final human visual/usability review remains a user-feedback loop, not a blocker to the code merge.

### Browser course interaction contract

The authoritative catalog contains 23 registered W1–W9 tasks:

- 14 application/service tasks have learner-facing build/run or managed network-service interaction.
- 9 library/test-focused tasks are intentionally exercised through source editing, the registered CMake/GoogleTest build/test and formal acceptance path; they are not separate standalone applications.
- All 23 tasks are selectable in the browser, render their lessons and prerequisite/state metadata, and include task-first guidance.
- All 23 tasks retain the existing fixed learning-engine acceptance contracts. Build/Run/service demonstrations do not grant formal PASS.

### P8-C delivered capabilities

- Loopback-only browser workbench, catalog/lesson/hints, secure workspace file API and local launcher.
- Monaco multi-file editing with offline textarea fallback, autosave, dirty state, fixed build/run/check controls and source-fingerprint protection.
- Fixed-profile learner applications, bounded stdin and workspace-local persistence.
- Managed TCP Echo, TCP Chat and HTTP Server sessions with bounded lifecycle, logs, restart and cleanup.
- Read-only append-only acceptance history, prerequisite/state explanations and stale-evidence handling.
- Chromium on Ubuntu 24.04 verifies the 23-task catalog and representative deep journeys, compiler/acceptance recovery and dirty-source refresh recovery. API/process regressions cover service crash/restart and web-server shutdown on Ubuntu 22.04/24.04. The catalog sweep does not claim that all 23 exercises were solved in the browser run.
- Desktop/mobile responsive checks and screenshot evidence; course matrix generated from `course/tasks.tsv`.
- Beginner-first `Start here` sections across all 23 lessons and a Chinese first-task guide/glossary.

### Explicit remaining boundaries (not blockers for P8-C closeout)

- Windows 11 + WSL2 target-machine end-to-end acceptance remains separate (P8-E).
- clangd/GDB interactive debugging remains P8-D.
- Full accessibility audit and first-time learner comprehension testing remain future usability work; keyboard history/course navigation tests do not certify full WCAG conformance.
- Final six-artifact release package can be assembled when a release/package publication is requested; the repository contains its schema at `docs/P8_FINAL_DELIVERY_PACKAGE.md`.

**P8-C is closed as a merged implementation milestone.** Continue the main product line with tutorial usability and learner success, not more P8 phase splitting.

## Tutorial usability follow-up

- All 23 registered lessons include task-specific Troubleshooting guidance: a symptom, the relevant API or boundary, and a concrete next action.
- Seven hint documents are normalized to the existing three-level CLI/browser contract. Their deeper topics remain available inside Hint 3; Hint 1/2 and all substantive guidance are preserved.
- A real-engine integration check compares all 69 CLI/API hint bodies and verifies that reading hints does not write progress or alter workspaces. The content gate also rejects extra, duplicate or out-of-order top-level hint levels.
- The Chinese learning guide explains how to use task-specific recovery sections and the shared progressive hints.
- Tutorial wording follows the released starter and fixture behavior, including supplied examples, exact Router matching, GET-only static files and caller-owned incremental parser buffers.
- These changes preserve formal acceptance and the P8-C closeout. Structural checks remain review aids; learner comprehension is still NOT_TESTED.

### Learning navigation and recovery

Delivered through [PR #48](https://github.com/clonetimer/CppAppLab/pull/48), merge `ea16613c491330e8be5df9179287ffe5a85ed7e9`. [Final PR CI #1875](https://github.com/clonetimer/CppAppLab/actions/runs/44251132830) passed all 12 applicable jobs; [main push CI #1876](https://github.com/clonetimer/CppAppLab/actions/runs/44252172245) passed all five scheduled jobs. Conditional skips and manual repository-wide formatting were not counted as passes.

- The browser exposes six supplied interface headers across W2, W3, W4 and W6 as read-only course references. Source-file writes and fixed acceptance remain scoped to the original catalog allowlist.
- Exact available file paths in lessons and hints open the relevant editor tab. Reference content is copyable in Monaco and the offline editor without entering autosave.
- Start here, Troubleshooting and Acceptance have direct section controls. Long hints use a scrollable reading area with a return-to-lesson action.
- A persistent next-action guide explains build/run/check and recovery, names prerequisites for locked tasks, and offers the next available course after refreshed formal PASS.
- Busy task transitions and task-bound asynchronous responses protect result attribution. Saving changed PASS source refreshes prerequisite-derived states and visible acceptance history immediately.
- First run before build gives an actionable build instruction while unsafe-directory and stale-build checks remain active.
- Dedicated state/API regressions and focused Chromium journeys complement the existing full browser course acceptance. Learner comprehension and the separate P8-D/P8-E boundaries remain explicitly untested or future work as described above.

### Learning-session recovery and readable guidance

Delivered through [PR #49](https://github.com/clonetimer/CppAppLab/pull/49), merge `09079bf896f54d11280f0523723577fed38391bb`. [Final PR CI #1884](https://github.com/clonetimer/CppAppLab/actions/runs/44259853369) passed all 12 applicable jobs; [main push CI #1885](https://github.com/clonetimer/CppAppLab/actions/runs/44260868516) passed all five scheduled jobs. Browser evidence included 56 Node cases, the full Monaco journey and 11 focused learning cases. Conditional skips and manual repository-wide formatting were not counted as passes.

- Successful course/file selections are reflected in a validated bookmark URL. Refresh restores the selected course and file, while the welcome screen offers a named return to the last course and course navigation remembers the last valid file.
- Browser history navigation shares the existing busy and save guards. Failed destination metadata reads preserve the prior editor; workspace-read errors keep the textbook visible and provide an explicit retry.
- Optional Monaco initialization has one 8-second deadline and a manual basic-editor action. Late callbacks cannot replace an editor after fallback has settled.
- Existing lesson tables render as semantic tables with bounded, keyboard-reachable internal horizontal scrolling. HTML escaping and exact available-file links remain in place.
- Application nonzero results retain diagnostics without assuming every negative input means incorrect code. The calculator's documented division-by-zero case is explained without treating the wrapper exit status as the learner process's exit code.
- The course catalog, source/acceptance boundaries and prerequisite policy are unchanged. Real browser regressions cover the user flows; observed learner comprehension and actual WSL target-machine qualification remain separate outcomes.

### Chinese orientation inside the workbench

- The persistent **中文学习指南** control and the welcome page's four-step entry expose the existing `course/LEARNING_GUIDE_ZH.md` inside an independently scrollable dialog. First use, terminology, the learning route and common errors have direct section controls.
- A fixed, bounded, read-only guide endpoint serves the authoritative document without invoking the learning engine or creating a workspace. The guide remains readable when engine health/progress initialization fails, and failed guide reads have an explicit retry.
- All 23 registered course headings in the route become exact catalog-bound navigation controls. They use the existing course-selection path, preserve dirty work on save failure and allow locked-lesson reading without starting the task or bypassing prerequisites.
- Opening/closing help preserves the current task, file, editor, lesson/hint view, URL and running operation. Native dialog focus, Escape, local section scrolling and focus return are covered separately from the underlying lesson controls.
- Browser-first instructions distinguish course selection from workspace creation; the welcome sample and first-use instructions include the first task's required trailing newline. No duplicate course metadata, general document-path API or extra curriculum is introduced.
- Local verification covers 38 server tests and 71 Node cases. The existing Chromium gate covers real DOM navigation, desktop/390 px layout, read retry and dirty-work protection; exact final-head CI and main-push closure evidence belong to the corresponding PR.


### P8-E — Windows/WSL2 delivery (implementation merged; target-machine acceptance pending)

- PR [#51](https://github.com/clonetimer/CppAppLab/pull/51) merged as `5e2a689f985779574431a1498327f685942549ae`; Windows double-click launch now defaults to the browser workbench while retaining `-Cli` and `-Setup`.
- Final PR-head CI [#44278244781](https://github.com/clonetimer/CppAppLab/actions/runs/44278244781) passed all applicable jobs, including Windows PowerShell 5.1/7, Ubuntu 22.04/24.04 build/package, GCC/GoogleTest, ASan/UBSan, clang-tidy, offline dependency closure and Chromium browser acceptance. The Ubuntu 22.04 source-build compatibility job is push-only and skipped on the PR event.
- Post-merge main push CI [#44279663585](https://github.com/clonetimer/CppAppLab/actions/runs/44279663585) passed all five scheduled jobs, including Windows launcher, Chromium, Ubuntu 22.04/24.04 package builds and Ubuntu 22.04 source-build compatibility. GCC/sanitizer/clang-tidy/offline bundle matrices are PR-only and therefore skipped on this main push.
- Added `tests/windows-target-machine-acceptance.ps1` to exercise the real Windows 11 + WSL2 launcher, verify the Windows default-browser handoff, query the CppAppLab health identity and 23-task catalog, and save timestamped JSON/transcript evidence under `artifacts/windows-target-machine/`.
- The target-machine script is syntax-checked by hosted Windows CI under PowerShell 7 and Windows PowerShell 5.1. Its actual execution on the user's Windows 11 desktop remains **NOT RUN** until the user runs it locally; hosted CI is not a substitute.
- PR [#52](https://github.com/clonetimer/CppAppLab/pull/52) merged the first target-machine harness as `f791d71aaf999c0dc3d4ccd990601ae30e48b4b4`; post-merge main CI [#44282863590](https://github.com/clonetimer/CppAppLab/actions/runs/44282863590) passed all five scheduled jobs. PR [#53](https://github.com/clonetimer/CppAppLab/pull/53) tightens the evidence: it reads the actual launcher-reported port and requires the user to confirm the welcome page is visibly open, rather than treating a successful `cmd.exe start` handoff as proof. Final validation of these refinements is in progress.
