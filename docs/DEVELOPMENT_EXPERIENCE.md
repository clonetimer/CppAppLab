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
- Every learner-provided source compiled by an application profile must be declared by that task. Reject undeclared, missing, symlinked, or unsafe learner sources. Any platform-owned support sources must be fixed, explicitly enumerated and documented in that task's build contract.
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

## 45. First-use documentation must lead to the delivered learning interface

- **Changed:** Moved the existing browser start command, local address, Chinese guide entry and first-exercise build/run/acceptance steps to the opening README and quick-start sections. Kept the extracted learner package's `./cppapplab` entry explicit.
- **Why:** The workbench and its guide already existed, but the repository's first instructions sent learners directly to the terminal and the browser steps were buried below setup and troubleshooting. Surface the available path before adding another launcher or onboarding feature.
- **Pitfall:** `docs/QUICKSTART.md` also ships inside the CLI learner archive, which does not contain `web/` or `scripts/start-web-workbench.sh`. Identify the built-source prerequisite, preserve the package path, and copy actual Chinese control labels from the UI. A new heading should not imply a new runtime or package capability.

## 44. Native modal keyboard checks must distinguish browser controls from page content

- **Changed:** Corrected the guide's real-browser keyboard assertion to allow focus to leave the document for browser controls, while requiring the next Tab to return to the guide and rejecting attempts to focus the background course search. Log the active element and document-focus state at the boundary so failures identify what received focus.
- **Why:** PR #50's initial PR/push runs (`44266757612` / `44266751170`) failed the same Shift+Tab assertion; all 71 Node cases and the other 13 focused browser scenarios passed. The assertion assumed native modal focus could never leave the document. [Chrome's native-dialog guidance](https://developer.chrome.com/docs/modern-web-guidance/guides/accessibility/accessibility#12-modals-and-native-dialogs) explicitly permits browser chrome navigation and advises against adding a native-dialog focus trap.
- **Pitfall:** `dialog.contains(document.activeElement) === false` alone does not establish that an inert background control received focus. Accept only the default BODY/HTML active-element fallback with no explicit tabindex or editable state, or an element inside the guide; keep actual background-focus and keyboard-return checks. Log document.hasFocus() for diagnosis rather than treating it as authoritative under browser focus emulation. Do not change product behavior merely to satisfy a stronger, unsupported test assumption.

## 42. Existing learner guidance needs a reachable product entry

- A maintained Chinese guide in the repository or CLI is not automatically available to a web learner. Trace the path from the welcome page and an active lesson to first-use instructions, terminology, course summaries and error recovery before writing duplicate content.
- Expose the authoritative document through a fixed read-only route and the same bounded, regular-file, UTF-8 checks as lessons. Reading help must not run the learning engine, create a workspace or depend on successful progress initialization; test it while engine calls are unavailable.
- Reuse the escaped Markdown renderer. Guide section controls should scroll and focus headings inside their reading area, because the URL fragment already identifies the learner's task and file.
- Promote only exact catalog IDs in the guide's task headings to course buttons. Do not infer lesson filenames from IDs, activate arbitrary Markdown URLs or create another course registry. Test all 23 IDs against the current catalog and leave unknown/future IDs, prose and code as text.
- Source instructions must match the entry point. Browser Continue selects a lesson and Start creates/opens its workspace; CLI Continue creates the workspace. Explain both explicitly instead of presenting terminal commands as a required browser step. Match the first visible example to the exact first-task output, including its newline.
- Structural audit flags are review cues. The five missing mental-model markers already had useful explanations under different headings; connecting existing guidance to learners is more useful than renaming text to increase a diagnostic count.

## 43. Contextual help has its own presentation lifecycle

- Help should remain available during a running operation and while code is dirty. Opening or closing it must not call course selection, save files, stop a service, reset hints/history or alter the learning URL. Only an explicit course choice uses the existing save/busy navigation guards.
- Use the native modal dialog for focus containment, a visible close button, Escape and focus return. Keep the underlying Escape handler inactive while the guide is open so one key does not also collapse history. After a successful course choice, focus the new course heading rather than a welcome button that has become hidden.
- Thread guide-owned autofocus behavior through workspace/file loading. A session check after awaiting course selection is too late if the nested file loader already focuses the editor; reproduce this with an existing workspace and a delayed source GET, not only a locked lesson. Preserve direct file-selection autofocus.
- A first-use entry should reopen at its promised instructions while the persistent guide entry retains reading position. If initial bookmark navigation hides the welcome opener while help is open, restore focus to the visible topbar entry.
- Deduplicate pending guide reads and cache the successful document for the page session. A late read can fill the cache but must not reopen a closed guide or steal focus. Give each opening a session identity so a pending course choice cannot close a subsequently reopened guide.
- Show failed reads and rejected navigation inside the modal: a page-level toast behind the dialog is not a readable explanation. Retry only the failed guide GET. A busy operation or failed save should leave the guide, active course and unsaved source available.
- Enhance guide task entries again when the catalog arrives; help may have loaded before engine startup completes. Reading the guide should not need a second request merely to acquire course navigation.
- Combine state tests of real app functions with actual-browser evidence for keyboard focus, both entry points, desktop and 390 px scrolling, final-content reachability, tables and preserved dirty code. A native dialog call in a fake DOM does not prove its layout or focus behavior.
- Deferred formatting edge: a future task heading combining strong and inline code around a registered ID would currently become a course control. The maintained guide has no such heading and navigation remains catalog-bound; future renderer work should preserve that nested code example as text.

## 39. Restore presentation context only after navigation actually succeeds

- A saved source file and a restored learning session are different outcomes. Verify refresh on a non-first file without manually reselecting the course; prior tests that clicked the course again hid the missing restoration.
- Keep task/file presentation identifiers in a validated URL and a small optional recent-position record. Resolve task IDs through the current catalog and files through the returned allowlist. Never cache source, acceptance, tokens or inferred progress in browser storage.
- Treat unavailable/corrupt storage and stale bookmarks as recoverable input. An invalid file must not become an arbitrary file GET; use an available file and explain the fallback.
- Fetch destination metadata before clearing the current editor. A failed save or destination request must retain the old course, source and URL; browser history navigation must obey the same busy/save guards as visible buttons.
- Same-course file links must reuse the current workspace and preserve its editor, diagnostics and running service. A failed target file read must not become a whole-course reset. After an initial source-read failure, disable every file entry point until the explicit workspace retry succeeds; an editable buffer without a saveable workspace is a broken recovery state.
- Reject a known Back/Forward traversal by restoring its cursor, not by replacing the destination entry. Verify that Back can be retried after the busy/save guard clears and that Forward still returns to the original course. Preserve a pending bookmarked filename until a successful file list can validate it, including refresh before Start.
- A restored history cursor already identifies the last committed position. Do not rewrite it from a half-loaded destination; flush only a completed navigation write, then commit the destination after its read finishes. Exercise Back while a destination file-list request is still pending, not only during acceptance.
- Browser journeys must wait for actionable state, not only a result badge. Formal PASS appears before the subsequent catalog refresh releases the busy guard; bookmark navigation must wait until the next course is available and its navigation control is enabled. Keep the busy guard and correct the test's readiness condition.
- A repeated course title is not proof that a reload/reselection finished. Before sending keyboard input to Monaco, wait for the current file and navigation controls to be ready. Keep this readiness wait independent of source read-only status so deliberate reference-file edit attempts still test the original protection.
- Separate workspace-loading errors from textbook rendering. Keep the lesson visible and offer a GET-only retry; do not turn a transient read failure into an accidental start/reset operation.
- Keep existing prerequisite access policy explicit. Presentation recovery does not bypass the server's rule for opening a downstream workspace when an earlier prerequisite is no longer current.

## 40. Optional editor resources must not hold the local learning loop indefinitely

- Immediate CDN rejection and a request that never settles are different failure modes. A loader with only load/error callbacks can leave the entire task transition busy forever.
- Give the full loader/retry/AMD initialization one total deadline, not a fresh timeout per stage. The current workbench uses 8000 ms and an immediate basic-editor action while waiting.
- Settle exactly once and bind callbacks to their current attempt. Late loader or AMD success after fallback must not create another editor, overwrite current textarea edits or restart loading.
- Preserve existing readiness labels and distinguish a loading label from a successful editor label. A test that matches a status prefix must not accidentally accept the pending state.
- Reproduce timing deterministically at the DOM/CDN/timer boundary, then verify actual typing, saving and formal acceptance with a pending request in Chromium. Keep these Node regressions in a CI job that runs on both PR and push events, not only a conditionally skipped job.

## 41. Verify rendered instructional meaning and documented negative cases

- Audit the actual lesson renderer against released Markdown. Todo commands, connection policies and metric definitions used tables, but a paragraph-only renderer displayed their pipes and separators literally.
- Render the supported table subset semantically, retain the existing inline escaping and file links, and make horizontal overflow local to a labelled keyboard-focusable region. Check final-column reachability at 390 px rather than merely counting table tags.
- Preserve escaped pipes and code backslashes, keep fenced code untouched, and retain ordinary or malformed pipe-containing prose instead of silently consuming it as a table row.
- A documented negative input can correctly return nonzero. Preserve its diagnostic output and failed-result styling while asking learners to compare the outcome with the lesson, rather than unconditionally instructing them to change correct code.
- Distinguish the application's original exit code from the learning CLI wrapper's status. The calculator intentionally exits 3 on division by zero while its wrapper may return 1; do not label the wrapper field as the learner process's exit code.

## 36. Make lesson-referenced interfaces reachable without widening edit permissions

- Compare lesson reading instructions with the browser's actual file allowlist. An interface present in the learner template is not automatically available in the editor; six supplied headers across four tasks were missing from the visible file list.
- Expose supplied headers as explicitly read-only references from the fixed course template. Keep editable sources, their order, source fingerprints and formal acceptance unchanged. File-list and content reads must use the same bounded, symlink-rejecting eligibility rule.
- Enforce read-only behavior at both boundaries: reject PUT through the original server allowlist and prevent client dirty/autosave changes. A textarea's readOnly property does not stop an explicit setRangeText call in a custom Tab handler.
- Preserve copying, keyboard focus and a clean return to editable source. Opening a reference must not discard another file's unsaved changes or make global Save incapable of saving those changes.
- Turn only exact paths in the available file list into lesson buttons. Course prose is not authority to expose arbitrary filesystem paths.

## 37. Bind asynchronous feedback to its originating task and refresh dependent state

- Capture the task ID before launching an operation. Disable and guard task navigation while an operation or task transition owns the busy state, including the outgoing save, so a delayed PASS cannot be applied to another course.
- Coalesce overlapping saves for the same task/file and retain the source-version check. Autosave, explicit Save and navigation must not race older content against newer edits; a change made during a save remains dirty until its own save completes.
- Protect asynchronous help and file reads too. A late response must not reopen closed help or replace the currently selected course/file.
- Order catalog, history and service-status reads with request generations. Ignore superseded errors as well as results; invalidate service polls when a task changes or a mutating operation starts.
- Saving a previously accepted source invalidates more than one status pill. Refresh the authoritative task catalog and any visible history summary; retain historical records while reporting the current source as stale.
- Refresh saves from STALE too: restoring the exact accepted source can legitimately restore PASS from the existing fingerprint without another check. Let the engine decide, and verify that the history count stays unchanged.
- Use refreshed formal progress for a persistent next-course action. Build/run success is useful feedback but does not establish course completion. A failed progress refresh should offer refresh explicitly instead of retaining an unverified continuation.
- HTTP success does not establish valid progress: UNKNOWN states must remain unavailable, never become a completed curriculum. Keep refresh failures visible and refresh the open history panel when the learner retries.
- Reproduce these transitions through the real UI state functions with controlled request timing, then exercise the complete interactions in Chromium. Keep network adapters at the boundary and avoid asserting on a fake's own behavior.

## 38. Verify that help can be read and recovery actions can be followed

- API hint parity does not prove browser readability. A bounded parent with hidden overflow can clip a long hint even when every character reaches the page. Give help a bounded scrollable viewport and an explicit return action; verify the final section at desktop and mobile widths.
- Provide direct Start here, Troubleshooting and Acceptance navigation so task-specific guidance remains reachable inside long lessons.
- Separate normal first-use conditions from unsafe filesystem conditions. An absent build directory should instruct the learner to build; existing non-directory paths and dangling or live symlinks must still be rejected.
- Render plain compiler diagnostics without depending on Monaco globals. Offline editing must preserve the real compiler output, clickable source location and formal failure state even when editor marker APIs are unavailable.
- Acceptance output may repeat a compiler diagnostic across stages. A browser recovery test should follow the first actionable error and verify the recorded result rather than assume the diagnostic text is unique.
- Test recovery without fabricating formal acceptance: real CLI run-before-build checks preserve history, while UI-only reference fixtures live in temporary catalogs. The full browser journey retains the released prerequisite chain.
- Record environmental verification limits accurately. A local browser blocked by process/socket restrictions is not a product test failure; retain state/API regression evidence and complete layout and interaction verification on the existing Chromium CI runner.

## 34. Deliver progressive hints through the same reader contract in every interface

- Audit the content against the actual CLI, HTTP API and UI level limits. Seven courses contained Hint 4–10 even though both user-facing interfaces expose only levels 1–3. The CLI stopped Hint 3 at the next numbered heading while the web reader included the remaining text.
- Preserve instructional material when fixing the mismatch. Keep Hint 1 and Hint 2 unchanged, group the existing deeper topics under Hint 3, and use ordinary descriptive subheadings for those topics. This retained 23 previously unreachable CLI subsections without adding more interface controls.
- Require exactly three ordered top-level hint headings in the course-content gate. Merely finding headings 1, 2 and 3 does not detect inaccessible extra levels or duplicates.
- Verify the public readers with the real engine and HTTP API across all 23 tasks and three levels. Reproduce the old seven mismatches before accepting the 69 corrected comparisons; compare meaningful bodies while allowing the web reader's surrounding-whitespace trimming.
- Keep hint lookup read-only. Disposable test markers may identify a task, but no progress record, source edit, prerequisite unlock or formal PASS should result from reading guidance.

## 35. Give each lesson a concrete recovery path grounded in its released task

- Describe the symptom, the specific interface or boundary to inspect, and one next action. Read the provided starter, command-line front end and fixed fixture before writing advice; a generic troubleshooting heading is not enough.
- Distinguish implemented examples, intentional learner TODOs and platform failures. Do not tell a learner to implement a nonexistent TODO or run a missing helper script.
- Keep each milestone's scope precise: the initial static-file task is GET-only; the Router matches exact method/path pairs; incremental parsing reads a caller-owned accumulated buffer. Later course capabilities must not leak into earlier task instructions.
- For library/test-focused tasks, direct learners to the registered fixture and API rather than inventing a standalone application workflow. Preserve the existing formal acceptance criteria while making recovery easier to follow.
- Use elements the actual lesson renderer supports. In this workbench, headings, inline code and bullet lists render directly; task-specific recovery guidance should not depend on unsupported tables.
- Keep structural coverage separate from learner outcomes. All 23 lessons can have actionable recovery sections while learner comprehension remains NOT_TESTED until observed with learners.

## 32. Finish an authorized milestone and keep its evidence and status consistent

- Preserve an explicit user instruction to merge completed, verified work. Carry the existing PR through review, its required CI and the actual merge; do not repeatedly stop for the same acceptance permission. Handle later usability feedback as follow-up work.
- Update STATUS, ROADMAP, the implementation plan and the closeout report in one coherent change. Retire stale “in progress” lines and split completed checks from genuinely untested follow-ups.
- Record the PR head, GitHub's synthetic merge checkout and the actual main merge separately. A successful run proves its own tested tree, not an unrelated later revision.
- Select workflow evidence by event as well as commit. A helper that only returns pull-request runs cannot establish whether the merged-main push run exists or passed.
- Inspect job and step conclusions. Record manual/conditional SKIPPED gates separately, and name the real platform matrix: Chromium on Ubuntu 24.04; API/core tests on Ubuntu 22.04 and 24.04.
- Close the scope that is actually delivered: 23 browser-visible task contracts and representative deep journeys. Full keyboard accessibility, learner comprehension, target-machine WSL2 acceptance and versioned release publication remain separately tracked outcomes.
- Batch related documentation corrections and experience updates before running CI, so closure does not become a chain of avoidable tiny PRs.

## 30. Close a web-workbench milestone by stating the per-task interaction contract

- A course being browser-accessible does not mean every task needs a separate standalone application runner; library/test tasks still expose their registered Run / Tests action. Classify each catalog task by its actual learning contract: direct application, managed network service, or library/test-focused exercise.
- For application/service tasks, provide a fixed-profile runtime interaction; never expose arbitrary shell commands or learner-controlled executable/host/argv fields.
- For library/test-focused tasks, the correct browser workflow is edit declared sources → registered build/test → formal acceptance. Creating a fake standalone app for every library task would distort the learning objective.
- Report both the total browser-accessible catalog count and the runtime-profile count. In this course, all 23 tasks are browser-accessible; 14 have application/service runtime interactions and 9 are library/test-focused. The 14 comprise Hello World's direct runner, ten structured application profiles and three managed service profiles.
- Close the implementation milestone when the integrated PR and required CI pass, while explicitly listing separate future work such as target-machine WSL2 acceptance, full accessibility review and learner comprehension testing.

## 29. Generate course coverage from the catalog and keep evidence dimensions separate

- The `course/tasks.tsv` file is the single authoritative source for task IDs, lesson paths, source files, acceptance kind and prerequisites. Never maintain a manually duplicated course matrix.
- Generate the course matrix in CI from that catalog and upload it with browser screenshots so the evidence is tied to the exact tested commit.
- Distinguish catalog/browser representation, deep interaction journeys and formal acceptance. A task being listed, selectable or having a `Start here` section is not proof its full exercise was solved.
- Fail generation when catalog rows are malformed or the expected task count changes unexpectedly; a count change should trigger an explicit course-scope review.
- Use explicit `NOT_RUN`/`SEE_FEATURE_MATRIX` states for evidence that the generator cannot prove. Never infer PASS from source-file presence or test-name presence.

## 28. Apply one task-first entry pattern across the full curriculum

- Once a consistent first-screen teaching pattern is proven in the introductory lessons, extend it to every catalog task rather than leaving advanced weeks as an abrupt jump in difficulty.
- Tailor the first actions to the actual task: one child process, one worker/future, one TCP exchange, one HTTP message, one parsed request, one route, one persistent connection, one timeout, one shutdown, one metric, one chunk, one target and one confined file.
- Keep each first path executable and incremental, then direct the learner to the existing detailed requirements and edge cases. Do not weaken security, concurrency or protocol acceptance criteria.
- Make the authoritative `course/tasks.tsv` lesson path the source for coverage. Chromium must visit all 23 task pages and assert the first-step guidance is rendered, in addition to the catalog metadata checks.

## 26. Put a task-first walkthrough inside the lesson, not only in a global guide

- A global beginner guide is useful but learners often arrive directly at a selected task. Put the immediate next actions inside the lesson itself.
- The first screen of a lesson should answer: what am I building, which file do I change, what is the first small step, how do I run it, and what does success look like?
- Keep advanced interface contracts, persistence limits, ownership rules and security constraints in the rest of the lesson. Lowering the barrier must reorder and explain material, not delete engineering depth.
- For cumulative tasks, sequence the work from a small working path to edge cases and robustness. Explicitly tell learners which complexity can wait until the basic loop works.
- Verify the rendered lesson in Chromium and assert that task-first guidance is actually visible for the first beginner lessons; markdown-file existence alone is insufficient.

## 24. Lower the first-use barrier with a task-first path and a plain-language glossary

- Beginners should not have to understand the full course architecture, task IDs, build systems or acceptance model before they can complete the first exercise.
- Put a short first-task walkthrough before the W1–W9 curriculum map: continue learning → start workspace → edit one named file → build → run → formal check.
- Explain the three controls separately. Build answers whether source can become a program; Run/Test answers what the program does; Formal Acceptance records whether the fixed course contract passes.
- Explain terms at first use or in a compact glossary (source, workspace, build, run, test, check, hint, fingerprint, prerequisite). Keep advanced details available after the first successful task rather than removing them.
- Normalize compiler errors as part of learning: teach the learner to inspect the first error, file and line, fix one issue, and request progressive hints before searching unrelated files.
- Validate tutorial changes with a first-time learner path in Chromium and keep the CLI guide and browser guide semantically consistent.

## 27. Re-select the task before asserting task-scoped UI after a full reload

- A browser reload reconstructs the workbench and may return to its default task selection; persisted workspace files and course state can be correct while the task subtitle/panel still reflects the default view.
- Separate persistence assertions from selected-view assertions: first query the workspace API to verify saved file content, then explicitly select the task and wait for its breadcrumb and authoritative state before checking task-scoped labels, prerequisites or diagnostics.
- Do not treat an unselected task's stale or empty subtitle as evidence that prerequisite metadata was lost. The browser test must establish the UI context it is asserting.

## 25. Browser editor persistence tests must account for editor-managed indentation

- Monaco can apply auto-indent when a test replaces source through keyboard input. The persisted source may therefore differ in leading whitespace even though the intended code edit is correct.
- For a refresh-persistence assertion, compare semantic source content while normalizing only leading indentation per line; do not remove or normalize tokens, literals, operators, line order or line endings inside the source.
- Keep the test responsible for two separate facts: the saved source survives reload, and the resulting task state reflects source-fingerprint invalidation. If exact byte preservation is itself a product requirement, test it through a deterministic editor API edit rather than keyboard insertion with auto-indent enabled.

## 23. A dismissed beforeunload dialog cancels navigation, not just the reload request

- In Playwright, `page.reload()` waits for the requested navigation lifecycle event. If a `beforeunload` dialog is dismissed, navigation is cancelled, so `domcontentloaded` will never fire and the default 30-second navigation timeout obscures the actual expected behavior.
- Attach the dialog handler before initiating reload, assert that the dialog type is `beforeunload`, dismiss it, and bound the navigation wait with a short timeout whose cancellation is expected.
- After cancellation, verify the current document and dirty editor remain usable, then explicitly save and perform a normal reload to prove persistence.
- Do not accept the dialog in this test: accepting would discard the edits and would not validate the learner-safe cancel path.

## 22. Browser refresh must protect dirty source and prove explicit-save recovery

- A browser refresh is not a successful recovery test if it silently discards an unsaved editor model. Assert that a dirty source triggers the real `beforeunload` prompt.
- Dismiss the prompt and verify the current editor/task remains available; then explicitly save through the same keyboard shortcut the learner uses.
- Wait for the UI's saved state and verify the source through the workspace file API before reloading.
- Reload only after the save completes, re-read the source from the workspace and compare the exact content. If the task previously had PASS evidence, the source change must make that evidence STALE rather than preserving a false PASS.
- Keep the browser-dialog interaction in Chromium; a unit test of the `beforeunload` listener alone does not prove browser navigation behavior.

## 21. Web-server shutdown is a service-session lifecycle boundary

- A service supervisor cannot rely only on user-initiated stop or idle expiry. The HTTP workbench server's `server_close()` path must stop all registered service sessions and terminate/reap their process groups.
- Test this by starting a real supervised child, invoking the actual server shutdown hook, and asserting the child has exited, the session is STOPPED, and the janitor thread is signalled.
- Make shutdown idempotent because normal test teardown and production shutdown paths may both call `server_close()`.
- Keep active browser request interruption as a separate test: stopping service children does not prove that in-flight API handlers, autosave requests or browser refreshes recover correctly.

## 20. Keyboard focus and service-crash recovery are explicit workbench contracts

- When a modal-like history panel opens from a keyboard action, move focus into the panel; Escape closes it and returns focus to the invoking control. A close button alone is not sufficient keyboard support.
- Verify Enter/Escape behavior in a real browser journey and assert the underlying task state is unchanged.
- A service supervisor must detect an unexpectedly exited child from `poll()`, expose FAILED plus its exit code in status, and permit a new start without leaving the stale process/session in the active set.
- Test the crash path by killing the real supervised child, polling the public status endpoint until FAILED, then restarting and verifying a new live process/port. A mocked `Popen` does not validate child lifecycle behavior.
- Keep interrupted web-server shutdown and browser refresh during an active request as separate recovery gates; do not claim them covered by a service crash test.

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

## 31. Audit lesson usability structurally, but never turn headings into a comprehension score

- Generate a lesson audit from the authoritative `course/tasks.tsv` so lesson paths, hint paths, source files, acceptance kinds and prerequisites cannot drift from the course catalog.
- Record observable structural signals separately: task-first entry, goal, concrete steps, mental model, common-error guidance, success criteria and hint-file presence.
- Mark learner comprehension as `NOT_TESTED` until an actual learner walkthrough or moderated review is performed. A heading or keyword match is only a review cue, not a PASS.
- Publish the audit as a CI artifact beside the course matrix and screenshots. This makes missing teaching scaffolding visible without pretending to measure teaching quality automatically.
- Use the audit to prioritize human review: start with lessons missing a first action, an understandable success criterion or a path from the first working example to advanced edge cases.

## 33. Lower tutorial friction by verifying explanations and copyable examples against the real contract

- Inspect the rendered teaching structure as well as heading presence. A Goal heading immediately followed by Start here is empty even when a structural audit counts it; place the heading beside its actual goal text.
- Trace technical mental models to the provided API, reference implementation and fixed fixture. TCP Echo consumes arbitrary bytes until write-side shutdown; the HTTP parser examines a caller-owned accumulated buffer; a shared_ptr capture makes the packaged-task wrapper copyable.
- Preserve learner complexity and acceptance requirements while correcting misleading explanations. Library design, byte boundaries, move-only callables and consumed-byte reporting remain part of the task.
- Check Markdown and shell escaping at the point learners copy it. Literal backslash-n between Markdown blocks is not a paragraph break, and a doubled shell continuation backslash does not join the intended compiler arguments.
- Execute copyable examples in disposable directories. This pass compiled the Calculator starter using the documented command and verified the HTTP document-root example writes the intended HTML plus one newline.
- Compare diagnostic audit rules with existing course conventions before treating a NO as a missing teaching element. The Router lesson already had a Goal and source section; matching Goal at a word boundary fixes that false negative.
- Keep structural signals, semantic review and learner comprehension separate. A corrected report still records comprehension as NOT_TESTED until an actual learner walkthrough provides evidence.



## 46. Windows first launch should land in the browser workbench

- **Changed:** Made the Windows double-click launcher start the local browser workbench by default, including after first-time setup. Added `-Cli` as an explicit compatibility path to the terminal home, and `-Setup` continues to rebuild and run CTest before launch. The WSL launcher requests browser opening only after the loopback service passes its health check; it prefers the Windows default browser and falls back to `wslview`, otherwise printing the exact local URL.
- **Why:** The browser-first workbench was available but Windows learners still landed in the terminal and had to discover and run a second command. The delivered product entry should match the primary learning interface.
- **Pitfall:** Do not open the browser before the service is ready, do not bind beyond loopback, and do not treat browser auto-open failure as service failure. Preserve stdin for first-use package prompts, preserve the explicit CLI path, and keep setup/build/test behavior unchanged. Port conflicts remain visible with a concrete alternate-port instruction rather than silently killing or reusing an unrelated process.
- **Regression:** Windows launcher smoke verifies the encoded bootstrap, distribution and setup arguments plus the default browser-mode argument; existing Ubuntu build/package and browser CI gates remain required before merge.

## 47. Test WSL browser interop independently from WSL process handoff

- **Changed:** Added a deterministic shell integration test for the post-health-check browser action. It verifies that the health payload identifies CppAppLab (not merely an arbitrary HTTP 200), the exact Windows `cmd.exe /c start` argument vector, the `wslview` fallback, and that browser opening is opt-in for non-Windows/manual launches. The test uses a fake local health response and never starts or signals a real service.
- **Why:** The Windows launcher handoff test proves PowerShell arguments reach WSL, but it does not prove the final URL is handed to the Windows browser correctly. These are separate boundaries and need separate evidence.
- **Pitfall:** Keep the test isolated in a temporary checkout, fake only the external interop/health boundaries, and assert the exact URL/argument order. A generic health endpoint can belong to an unrelated service occupying the default port; require the expected engine identity and a valid task count before opening it. Treat malformed JSON as an unhealthy endpoint without leaking a traceback into learner-facing startup output. Do not claim that a stub test proves browser launch on a real Windows desktop; retain P8-E target-machine validation as a separate NOT RUN item until executed there.
