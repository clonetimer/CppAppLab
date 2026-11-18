# Chinese learning guide in the web workbench

## Goal

Let a beginner reach the maintained Chinese orientation, terminology and course route from the web workbench, then open the intended registered lesson without losing current work. The authoritative source remains `course/LEARNING_GUIDE_ZH.md`.

## User-visible behavior

- A persistent **中文学习指南** button and a welcome-page first-use entry open an independently scrollable native dialog. Opening and closing it preserve the active task, file, code, hint/lesson view, URL, running operation and acceptance state.
- The dialog exposes **第一次使用**, **常用词解释**, **学习路线** and **常见错误** section shortcuts, a visible close action, Escape support and focus return to the invoking control. Section navigation scrolls and focuses a heading inside the dialog without changing the URL fragment.
- Loading and failure messages live inside the guide, with an explicit read retry. The guide can be read even when engine health or progress loading failed.
- The guide's 23 registered task headings become course buttons only when their exact IDs appear in the authoritative catalog. They reuse `selectTask`: dirty files are saved first, busy navigation is rejected, locked lessons remain readable and only successful selection closes the guide. Merely reading the guide never starts a task, executes a program or writes acceptance evidence.
- The first-use text distinguishes the browser workflow from optional CLI instructions. The welcome code sample shows the newline required by the first lesson.

## Interfaces and limits

- `GET /api/v1/learning-guide` returns `{ "markdown": <authoritative UTF-8 text> }`. It reads only the fixed `course/LEARNING_GUIDE_ZH.md` path through the existing safe, bounded repository reader using `MAX_LESSON_BYTES` (64 KiB). No CLI invocation, workspace creation or arbitrary document path is accepted.
- Rendering uses the existing escaped Markdown renderer. Only a strong element at the beginning of a guide list item with an exact known task-ID token may become a button; ordinary code, unknown IDs and future curriculum stay text. No general Markdown link or raw HTML capability is added.
- Reading may be cached for the page session. Concurrent open/retry requests are deduplicated. A response arriving after close may populate the cache but must not reopen the dialog or steal focus.
- Course navigation errors remain visible in the open guide. Native modal focus containment is retained while navigation is pending; a guide-only loading/error state does not own the workbench busy state.

## Scope

This is a web learning usability batch. Existing curriculum, acceptance, prerequisites, task catalog format, P8-C closure and editor/service contracts remain authoritative. Chinese search aliases, full lesson translation, a full accessibility audit, clangd/GDB, target-machine qualification and releases are outside this batch.

## Acceptance evidence

1. API tests prove exact source equality, fixed-path access, bounded/regular-file/UTF-8 handling, and independence from the learning engine and workspace mutation.
2. Node tests exercise the real app functions for loading, retry, close/reopen, active-work invariants, exact catalog entry enhancement and guarded course selection.
3. The existing real Chromium suite covers both welcome and course entries, desktop and 390 px layout, keyboard section navigation, focus return, preserved dirty editor state, locked-course reading, busy/save-failure behavior, and guide retry without clearing the lesson.
4. One independent whole-branch review, required PR CI, screenshot inspection and actual main-push CI close the batch. Development experience and status are updated together.
