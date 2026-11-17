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
