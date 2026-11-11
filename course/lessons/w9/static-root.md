# W9-A3 · Secure Static-File Root Confinement

## Goal

Replace path-string concatenation with descriptor-relative file access. The document root is opened once and retained as a directory file descriptor. Every path component is resolved relative to an already-open directory descriptor; symlinks are never followed.

## Required security properties

- Parse the raw request target with the completed W9-A2 parser. Do not strip query strings or percent-decode again in the file handler.
- Serve GET only. Return 405 with Allow: GET for other methods.
- Accept only the canonical decoded origin-form path returned by the request-target parser.
- Map / and paths ending in / to index.html within the corresponding directory.
- Reject empty interior path components, dot segments, backslashes and malformed request targets.
- Open the configured document root once with O_DIRECTORY, O_CLOEXEC and O_NOFOLLOW. Keep its descriptor alive for the handler lifetime.
- Traverse intermediate components using openat(parent_fd, component, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW).
- Open the final component using openat(parent_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK).
- Use fstat on the opened descriptor and serve regular files only. O_NONBLOCK prevents opening a FIFO from blocking before the type check.
- Enforce the configured maximum file size both from fstat metadata and while reading, so a file that grows after the initial size check cannot exceed the memory limit.
- Read bytes from the already-open descriptor. Never reopen by a reconstructed absolute path.
- Keep the descriptor-based root stable if the document-root directory is renamed after handler construction.

This policy rejects all symlinks below the document root, including symlinks that point to another location inside the root. That is intentional: it keeps the first secure implementation easy to audit. The document-root path itself is trusted configuration and its final component must not be a symlink.

## Why string-prefix checks are insufficient

A lexical check can reject ../ in the request string but still be bypassed by symlinks or by a path changing between a check and an open. A directory file descriptor plus openat/O_NOFOLLOW makes each lookup relative to a directory that has already been opened and verified. Once an intermediate directory is open, renaming or replacing its pathname does not redirect the descriptor.

## Learner task — w9.static-root

**Workspace files**

- include/http_message.hpp (completed W8 message model)
- include/http_request_target.hpp and src/http_request_target.cpp (completed W9-A2 parser)
- include/static_file_handler.hpp
- src/static_file_handler.cpp

The completed message and request-target components are staged as known-good baselines. Implement the static file handler's root descriptor ownership, safe component traversal, regular-file verification, bounded read and HTTP responses.

**Fixed acceptance:** 34 platform-owned GoogleTests in course/fixtures/w9/static-root. They cover normal and binary files, URL decoding, query isolation, symlink escape, FIFO handling, size limits, descriptor lifetime and invalid request targets.

\`\`\`bash
./build/cppapplab start w9.static-root "$HOME/cppapplab-work/w9-static-root"
./build/cppapplab lesson "$HOME/cppapplab-work/w9-static-root"
./build/cppapplab hint "$HOME/cppapplab-work/w9-static-root" 1
./build/cppapplab check "$HOME/cppapplab-work/w9-static-root"
\`\`\`

## Explain your design

1. Why must the handler use the decoded path from W9-A2 without decoding it again?
2. Why does O_NOFOLLOW need to be applied to every path component, not only the final file?
3. Why are intermediate components opened with O_DIRECTORY?
4. Why is a directory file descriptor more reliable than concatenating document-root and request paths?
5. Why is O_NONBLOCK useful when opening a final component before fstat?
6. Why must the handler verify S_ISREG after opening the descriptor?
7. Why should the file-size limit be checked both before and during the read?
8. Why does an open directory descriptor remain anchored when its pathname is renamed?
9. Why does this task reject even symlinks that point inside the document root?

## Acceptance

- [ ] The request target is parsed once using the W9-A2 parser.
- [ ] Root and trailing-slash paths map to index.html.
- [ ] Only regular files are served; directories, FIFOs and devices are rejected.
- [ ] Every intermediate directory and final file is opened with O_NOFOLLOW.
- [ ] Symlink traversal cannot escape the configured document root.
- [ ] File bytes and Content-Type are preserved.
- [ ] The maximum file size is enforced before and during reading.
- [ ] The root descriptor remains valid after its directory is renamed.
- [ ] Invalid targets return 400, missing/unsafe paths return 404, oversized files return 413 and non-GET methods return 405.
- [ ] All 34 fixed checks pass.
