# Development Experience and Regression Rules

This document turns recurring implementation failures and successful fixes into reusable engineering rules. New work should extend these rules with a reproducible failure, its root cause, the corrective design, and a regression test.

## 1. CI evidence must be interpreted per job and per step

- Inspect the exact pull-request head SHA and its associated workflow run. A successful older push run does not qualify a newer PR head.
- Record each job conclusion and important step conclusion. A skipped job or step is not a pass, even if the overall workflow conclusion is successful.
- Keep manual/opt-in checks distinct from required CI. In particular, a skipped repository-wide formatting step must remain visible as skipped; a successful clang-tidy build does not prove that the full formatting check ran.
- Hosted CI does not replace target-machine acceptance. Windows PowerShell syntax and a simulated WSL process handoff are not proof of a complete Windows 11 + WSL2 runtime installation.

## 2. Never put large or sensitive request payloads in process arguments

**Observed failure mode:** Linux limits the size of an individual argument (commonly around 128 KiB), and command-line arguments can be exposed through process inspection.

**Required design:**

- Keep executable paths and argument vectors platform-owned and finite; do not invoke a shell or accept arbitrary browser-provided commands.
- Send HTTP request text and other large payloads through child-process standard input, not argv.
- Enforce the same explicit input limit at the HTTP/API boundary, the C++ CLI boundary, and the core process runner. The current HTTP parser/router contract is 256 KiB.
- Close stdin after the request is delivered so stdin-driven programs receive EOF and can finish parsing deterministically.
- Test a real subprocess payload larger than the Linux per-argument limit (140,000 bytes) and separately reject an input larger than 256 KiB. A mocked process call alone is insufficient evidence.

## 3. Child-process lifecycle must be bounded and recoverable

- Use a fixed argv vector with no shell expansion.
- Bound execution time and captured output.
- Place children in a process group so timeout cleanup can terminate descendants, not only the immediate process.
- Poll and drain stdout/stderr while checking child completion; reap the child with waitpid on every success and error path.
- Use a bounded temporary input source for stdin when a large payload is required; validate its size before forking.
- Add tests for timeout, output limits, invalid input, process-group termination, and cleanup. Avoid tests that only prove the happy path.

## 4. Preserve interactive WSL terminal input

**Observed failure mode:** a first-use installer can clone and build successfully but lose the user's confirmation input when a PowerShell-to-WSL bootstrap redirects or consumes stdin before the interactive prompt.

**Required design:**

- Preserve the controlling terminal across the Windows PowerShell → native wsl.exe → Bash bootstrap boundary.
- Read first-use confirmation from the controlling terminal (for example, /dev/tty) when stdin is used for bootstrap payload transport.
- Do not assume a redirected stdin stream is an interactive terminal; detect the condition and provide a clear diagnostic.
- Test the real PowerShell launcher against a Windows-side wsl.exe stub, including a checkout path containing spaces, setup mode, and the exact native-process argument handoff.
- Keep PowerShell 5.1 and PowerShell 7 syntax checks. Simulated handoff tests and actual WSL2 target-machine acceptance remain separate gates.

## 5. Browser acceptance must exercise the real learning engine

- Use Chromium/Playwright against the actual local web service and compiled C++ learning engine for core build/run/acceptance paths.
- Verify real source editing, autosave, multi-file navigation, compiler diagnostics, build, execution, and formal acceptance as separate operations.
- Application build/run must never write formal acceptance evidence. Only the fixed `cppapplab check` operation records course evidence.
- Assert that a successful or failed application demonstration leaves the course acceptance state unchanged.
- Exercise incomplete learner implementations first and verify that their actual TODO/error is displayed. Do not silently replace learner code with a reference solution or report a false PASS.
- If a known-good reference implementation is needed for an end-to-end CI journey, stage it explicitly into a disposable test workspace after the incomplete-code assertion. This is test-fixture setup only; production workspaces must remain learner-owned.
- Keep a real 390 px mobile viewport check, assert no page-level horizontal overflow, and verify that the lesson precedes the editor on mobile.
- Capture screenshots at scroll origin (`window.scrollY === 0`) so sticky headers do not distort full-page evidence.
- Block the editor CDN in a dedicated test to prove the offline fallback remains usable and does not emit uncaught browser errors.

## 6. Distinguish application execution from a task's formal quality gate

W4 Quality Gate is an important example: its TaskManager application implementation is complete, while its learner-authored GoogleTest file intentionally contains TODO failures. Therefore:

- The W4 application profile should build and execute add/list/done/remove successfully against the learner's current runtime sources.
- The formal `cppapplab check` must still fail until the learner replaces the TODO tests with meaningful assertions.
- Do not infer that an application must fail merely because its task acceptance tests are incomplete. Conversely, a successful application demo must never be promoted to a formal PASS.
- Browser acceptance should exercise both paths and assert the expected state transition: application actions leave the task in progress; failed formal acceptance moves it to NEEDS WORK.

## 7. Application profiles are finite contracts, not a browser shell

- `course/tasks.tsv` and the C++ learning engine remain authoritative for task IDs, declared sources, prerequisites, workspaces, source fingerprints, and formal acceptance.
- Each browser application profile maps one task to a fixed set of actions and typed fields. The browser must not supply executable paths, compiler flags, CMake configuration, working directories, environment maps, or arbitrary argv.
- Every source compiled by an application profile must be declared by that task. Reject undeclared, missing, symlinked, or unsafe sources.
- Library/test-only tasks must not receive fabricated application runners.
- Long-running TCP/HTTP services require an explicit lifecycle contract (loopback bind, readiness, bounded sessions, stop, process-group cleanup, and idle expiry). Do not disguise a service as a one-shot CLI form.

## 8. Persistent application state belongs to the learner workspace

- Todo and TaskManager demos must store state below the active workspace's application runtime directory.
- Pass the workspace-local storage path through a scoped environment override; do not write to the user's global home-directory data file.
- Test isolation from a pre-existing global storage path and verify that a second workspace cannot observe the first workspace's records.
- Keep sample HTTP document roots inside the workspace and reject unsafe paths and symlinks.

## 9. UI state must follow task and workspace state

- Hide the application runner when switching to a task without a registered application profile.
- Show the profile schema for locked tasks only as an instructional preview; disable build/run until prerequisites and workspace readiness are satisfied.
- Rebuild action fields when the selected action changes, and disable controls during an in-flight operation.
- Save dirty source files before build/run; reject stale application binaries using the source fingerprint.
- Keep lesson, source editor, application controls, diagnostics, output, and formal acceptance visually distinct. Do not let an application result look like a formal course PASS.

## 10. Required regression record for future fixes

For each new development lesson, record:

1. Reproduction steps and affected platform/runtime.
2. Root cause, with the exact boundary or invariant that failed.
3. Corrective design and why it addresses the cause.
4. A regression test that fails before the fix and passes afterward.
5. CI run and exact tested commit SHA.
6. Any remaining target-machine or manual acceptance gate.

These rules are intended to reduce repeated debugging, prevent false acceptance, and keep the browser-first learning experience faithful to the real C++ course engine.


## 11. Long-running learner services need a real process supervisor

- Build the registered learner application first, then launch the workspace-owned `.cppapplab/application/learner_app`. Never launch the CppAppLab learning-engine binary as if it were the learner's server.
- Pass a fixed argument vector only. For network services, request an ephemeral port by passing `0`, then parse the service's flushed readiness line to obtain the actual port. Do not reserve a port, close it, and later assume it remains free; that creates a bind race.
- A process is not ready merely because `Popen` returned. Require an explicit readiness marker and fail startup with the learner's captured diagnostics when the marker is missing or the process exits.
- Drain subprocess pipes continuously. For a buffered text pipe, `read(4096)` can wait for a full buffer; use the underlying buffered stream's `read1` (or an equivalent non-blocking drain) so short readiness lines are observed promptly.
- Start each service in a new process session. On stop, expiry, startup failure, task switch, and web-server shutdown, terminate the entire process group—even if the group leader has already exited. Descendants can outlive their parent.
- Keep service logs bounded and continuously drained; otherwise a full stdout pipe can deadlock the learner service.
- Persist startup/build failures as session diagnostics. A later status refresh must not turn a visible BUILD_FAILED/FAILED result into an apparently clean STOPPED state.
- Keep lifecycle limits explicit: active-session count, idle expiry, maximum lifetime, readiness timeout, bounded logs, and deterministic cleanup.

## 12. Network-service browser bridges must be finite and loopback-scoped

- TCP Echo, TCP Chat and HTTP Server are distinct interaction contracts; do not present them as arbitrary terminal commands.
- Chat clients are created and owned by the local adapter. Bound the client count, nickname syntax, line/message sizes, per-client inbox and pending output. The browser sends typed actions, not raw socket destinations.
- The HTTP request bridge always connects to `127.0.0.1`. Allowlist methods, validate the request target after URL decoding, reject absolute URLs/authority/CRLF/traversal, bound request and response bodies, apply a short timeout, and return only a small response-header allowlist.
- Service templates should default to loopback binding. The browser bridge must never accept a user-supplied host or port. CppAppLab is a local development workbench, not an operating-system sandbox against deliberately modified learner source.
- A service demo, readiness check or successful HTTP response is not formal course acceptance. Only the platform's fixed `cppapplab check` operation writes acceptance evidence.

## 19. Cross-course browser coverage must be driven by the authoritative task catalog

- Keep the 23-task ID list in the Chromium journey aligned with `course/tasks.tsv`; assert exact count, unique presence and successful selection for every task.
- For each selected task, verify its authoritative detail API, lesson content, task state and prerequisite metadata. A sidebar entry alone does not prove the task is usable.
- Compare browser-visible state labels against the canonical `stateLabel` mapping; do not invent new UI states such as a separate 'needs recheck' label unless the learning engine actually emits them.
- Locked tasks must expose a non-empty prerequisite explanation; do not attempt to start or execute them just to make the test progress.
- Keep full catalog traversal separate from deep workflow tests: the former proves breadth, while targeted task journeys prove editing, build/run, acceptance, service lifecycle and recovery semantics.
- After a formal PASS, reload the browser and verify both the current state and append-only history remain available. Build/run/service operations must not add acceptance records.

## 18. History UI requires a real browser journey after a formal PASS

- An API unit test proves only the serialization/parsing contract. It does not prove that the learner can discover, open, read and close the history panel in the actual workbench.
- Add the history check immediately after a real successful formal acceptance in the Chromium course journey. Assert count, result, timestamp, workspace and source fingerprint, then verify the panel closes without changing the task's acceptance state.
- Keep history ordered by attempt time at the source; reverse only for newest-first presentation. Do not silently discard failed attempts or collapse repeated PASS records.
- A historical PASS must never be treated as the current state: always display the learning engine's latest state separately, because subsequent FAIL or source edits can invalidate the earlier pass.
- Exercise an empty history, multiple attempts, unknown task IDs and an unavailable history backend separately. Build/Run/service demonstrations must not add records.

## 17. Reuse the learning engine's canonical append-only history for browser timelines

- The authoritative course history already lives in the learning engine's per-task append-only history records. The browser should consume this source rather than inventing a second database or writing records from Build/Run/service actions.
- Expose history through a read-only API that calls the fixed `cppapplab history <task-id>` command and parses its bounded, task-scoped output. Validate task IDs against the course catalog before invoking the engine.
- Preserve the distinction between an attempt and current task state: a historical PASS can be followed by FAIL, and the latest state must not be inferred from the first or best historical attempt.
- Display timestamps, result, test count, workspace and source fingerprint so learners can compare attempts without exposing mutable evidence-writing controls.
- Explain STALE and LOCKED using the same state semantics as the learning engine. Never let a browser timeline independently grant prerequisites or mark a task passed.

## 16. Offline APT bundle downloads must distinguish transient and permanent mirror failures

**Observed failure mode:** the offline dependency workflow successfully resolved package metadata but both Ubuntu versions received HTTP 503 from the pinned Snapshot endpoint for `libc-dev-bin`; fallback mirrors returned 404 because they did not retain that exact snapshot package version. A single transient Snapshot outage therefore failed the entire offline bundle build.

**Required design:**

- Keep the repository-pinned Ubuntu snapshot and the signed APT SHA-256/MD5 metadata authoritative; do not silently substitute a newer package version.
- Retry transient HTTP 5xx, connection and timeout failures against the same URI with bounded exponential backoff before moving to the next configured mirror.
- Treat permanent HTTP 4xx responses (especially 404) as non-retryable for that URI and continue to the next configured mirror.
- Keep every download in a temporary `.part` file; publish the `.deb` only after size and signed checksum verification.
- Add deterministic local HTTP-server tests for 503 → 503 → 200 and permanent 404. Do not use live Ubuntu mirrors as unit-test dependencies.
- Preserve the distinction between a mirror outage and an unavailable pinned package: if all mirrors fail, report each URI and HTTP status without weakening version pinning or checksum verification.

## 15. Managed TCP clients need bounded blocking writes

- A socket configured with `setblocking(False)` makes `sendall()` unsuitable as a general bounded-message API: a partial write can raise `BlockingIOError` instead of completing the message.
- For adapter-owned Chat clients, use a finite socket timeout, use `select()` to wait for readable data, and retain blocking/timeout-mode `sendall()` for the small bounded protocol lines.
- Keep the client count, line size, message size, inbox length and operation timeout bounded independently. A socket timeout alone is not a substitute for those limits.
- Exercise multiple clients through the actual TCP protocol bridge, including nickname acknowledgement, join/leave broadcasts and message delivery.

## 14. Let the learner service own ephemeral port allocation

**Observed failure mode:** the TCP Echo server already supports binding to port `0`, but its reference CLI rejected `0` before the server constructor could bind. The service adapter therefore cannot safely assume that every service accepts a preselected concrete port.

**Required design:**

- Pass port `0` to the learner service and let the kernel allocate the listening port.
- Require the service to flush a readiness line containing its actual bound port; the supervisor validates and records that port before reporting `RUNNING`.
- Do not reserve a port in the adapter, close the reservation, and then launch a second process to bind it. That creates a time-of-check/time-of-use race.
- Keep port parsing consistent between learner starter and reference CLI entry points so the starter compiles and reaches the intended runtime TODO rather than failing on an unrelated argument check.
- Add a regression assertion that the launched learner process receives `0`, while the session status exposes a valid non-zero kernel-assigned port.

## 13. Keep Pimpl starter code compilable while preserving the TODO

When a learner-facing class owns `std::unique_ptr<Impl>`, a constructor that throws before `Impl` is defined can fail to compile: exception cleanup instantiates the member destructor and requires the pointee type to be complete. Define a minimal `Impl` in the starter translation unit before the throwing constructor. The application should compile and then report the intentional TODO at runtime; do not let an incomplete-type error masquerade as a learner implementation failure.
