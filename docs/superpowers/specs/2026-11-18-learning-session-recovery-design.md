# Learning Session Recovery and Readable Guidance

## Intent and scope

Continue the authorized tutorial-usability line on merged main `ea16613c491330e8be5df9179287ffe5a85ed7e9`. PR #48 and main CI #1876 are closed. Help learners return to their own saved work and keep reading/editing when an optional editor CDN stalls. Preserve the existing 23-task course, learner sources, prerequisite rules and formal acceptance.

## Evidence

- Startup loads the catalog only; selected course/file is not represented in the URL or remembered. Refresh requires finding the course again and opens its first source file.
- A failed destination metadata request clears the old editor first; a file-list failure replaces otherwise valid textbook content with the error.
- Monaco script/AMD loading has no bounded deadline. A pending request can retain the global busy state indefinitely.
- Existing Todo, connection-policy and observability tables render as pipe-separated paragraphs.
- Calculator's documented division-by-zero case intentionally exits nonzero, but the application UI unconditionally tells the learner to modify code.

## Chosen approach

The implementation choices below are engineering decisions under the user's standing authorization to continue and merge verified work; no new product approval is required.

- URL-only recovery supports bookmarks but loses the position when the launcher opens `/`; storage-only recovery cannot express a bookmark. Use a validated task/file fragment plus a small browser-local recent-position record. Store presentation identifiers only, never source, progress, acceptance or tokens.
- Keep the existing plain editor as the recovery path. Bound the entire Monaco initialization to 8 seconds and expose an immediate basic-editor action while loading; settle once and ignore late callbacks. Do not add editor dependencies or a separate editor service.
- Extend the current escaped Markdown renderer only for the table subset present in released lessons. Use semantic table elements and a bounded horizontal scroller. Preserve existing inline formatting and exact file links.
- Keep nonzero application results diagnostic; describe them neutrally and explain the calculator negative case without converting it to a formal or application PASS.

## Behavioral contract

1. A successful course/file selection updates `#task=<catalog-id>&file=<allowlisted-path>`. Course changes create history entries; file changes replace the current entry. Explicit valid URL positions restore on startup. Back/Forward and incoming fragments honor busy and save-before-navigation guards.
2. The welcome screen offers a named return to the last successfully visited course; revisiting a course restores its last valid file. Invalid/corrupt/blocked storage is harmless. Invalid task links do not call arbitrary task endpoints; invalid file links never reach a file GET and fall back to a listed source.
3. Navigation commits only after destination metadata succeeds. Failed saves or destination reads preserve the old lesson, file and route. Workspace-load errors stay separate from textbook content and offer an explicit retry. Existing/locked lessons do not auto-create workspaces just because they are bookmarked.
4. Reloaded work is read from the real workspace. The existing unsaved-change guard remains; this feature is not an offline source backup.
5. Stalled, failed or manually skipped Monaco loading resolves to the current textarea path. A later CDN callback cannot create an editor or replace source after fallback has settled.
6. Released lesson tables render with headers and cells, escaped inline content, keyboard-reachable internal horizontal scrolling and no page overflow at 390 px. Ordinary pipes and fenced code remain ordinary content.
7. Negative application completion preserves actual output and the fixed check/history boundary; the UI does not assert that every nonzero return means incorrect learner code.

## Boundaries and validation

No changes to CLI/API prerequisite policy, acceptance fixtures, catalog, editable-source declarations, P8-D tooling, target-machine WSL acceptance or release publication. Source restoration still follows the server's existing access rules. Actual learner comprehension remains unobserved.

Use focused failures in the real app.js through dependency-free Node boundary adapters, then the existing Chromium/real-engine pipeline for reload, history navigation, pending-CDN fallback, tables at 1440/390 and the calculator negative case. Local Chromium is unavailable because process socket creation is prohibited; do not retry with bypass flags. One independent whole-branch review precedes PR CI and authorized merge; verify the actual main push.
