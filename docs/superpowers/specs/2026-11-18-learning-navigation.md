# Learning navigation and recovery

## Goal

Make the released browser course usable from lesson reading through editing, recovery and the next lesson. Continue the closed P8-C workbench without changing course acceptance.

## Observed problems

- Six supplied headers across W2, W3, W4 and W6 are absent from the browser file list, although lessons require learners to understand these interfaces.
- Long hints have no independent scrolling or return control inside the bounded lesson panel; Troubleshooting has no direct navigation entry.
- After formal PASS, the visible workbench has no persistent action to enter the next available lesson.
- Course navigation remains active during asynchronous acceptance; its response can be applied to another selected course.
- Saving changed PASS source leaves prerequisite-derived catalog states and an open history summary stale until reload.
- Running a new workspace before building reports a missing-directory security error instead of the required next action.

## Required behavior

1. Append non-editable template headers to the existing workspace file list. Select only regular `.h`, `.hpp`, `.hh` and `.hxx` files below the fixed template's `include` directory, excluding declared editable sources. Both file-list and file-content JSON expose `read_only`. Read reference content from the registered course template. Keep PUT restricted to the existing `source_files` allowlist.
2. Label references clearly in both Monaco and the fallback editor. They remain selectable and copyable; typing, Tab insertion, autosave and explicit save never modify a reference. Switching back restores ordinary source editing. Existing dirty editable files remain saveable.
3. Make exact, available inline file paths in lessons and hints open the corresponding file. Unavailable paths remain ordinary text. Provide section actions for Start here, Troubleshooting and Acceptance.
4. Display hints in a bounded scrollable reading area with a return-to-lesson button. Desktop and 390-pixel mobile views can reach the final Hint 3 section. Closing or switching courses invalidates pending hint requests.
5. Keep a concise next-action guide visible in the workspace. Explain edit/build/run/check order, offer relevant recovery after failures, link locked tasks to their prerequisite, and offer the actual next available task only after authoritative formal PASS. Build/run success never implies course completion.
6. Bind operation results to the originating task. Disable course switching while an operation or task load is in progress, and guard the selection handler. Refresh prerequisite-derived states after accepted source becomes stale; refresh visible history while retaining its historical records. Failed state refresh must remain explicit and must not advertise an unverified next task.
7. Distinguish absent build directories from unsafe directories. Missing build state gives an actionable build instruction; symlink/non-directory rejection remains intact. Running never creates formal acceptance evidence.

## Constraints

- Keep all 23 catalog entries, editable `source_files`, fixed acceptance fixtures and prerequisite rules unchanged.
- Reference reads use existing bounded, symlink-rejecting repository path checks and the 1 MiB source limit. No arbitrary template-file browser is added.
- Use existing JavaScript, Python, C++17 and Playwright infrastructure; add no runtime dependencies.
- Keep user-facing guidance in clear Chinese and preserve current learner source and autosave behavior.
- The existing authorization to implement, verify and merge applies; user usability feedback follows delivery.
- Structural/browser checks do not claim first-time learner comprehension.
