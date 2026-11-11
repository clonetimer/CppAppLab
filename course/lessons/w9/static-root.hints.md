## Hint 1 — Own the document root descriptor

Create a small RAII file-descriptor wrapper. Open the configured root with O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, verify it with fstat and keep it alive in the handler. Do not store only the path string.

## Hint 2 — Walk one component at a time

Start with the root descriptor. For every intermediate component, call openat on the current directory with O_DIRECTORY | O_NOFOLLOW. Replace the current directory descriptor only after the new one opens successfully. Open the final component with O_NOFOLLOW and O_NONBLOCK.

## Hint 3 — Verify the opened object, then read that descriptor

Call fstat on the final descriptor and require S_ISREG. Check st_size against the configured limit, then read in a loop that handles EINTR and checks the cumulative byte count. Never reconstruct or reopen an absolute filesystem path.
