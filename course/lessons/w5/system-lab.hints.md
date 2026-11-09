## Hint 1 — Queue

Start with one mutex protecting the deque and the closed flag. Use `not_full` for producers and `not_empty` for consumers. A push wait predicate should allow progress when the queue is closed or the size is below capacity. A pop wait predicate should allow progress when the queue is closed or the queue is non-empty.

## Hint 2 — Descriptors and child setup

Create the pipe before fork. In the child, close the read end, duplicate the write end onto stdout and stderr, close the original write descriptor, build a null-terminated argv array, then call `execvp`. If exec fails, terminate with `_exit` rather than returning into the parent's C++ runtime.

In the parent, close the write end immediately. This is important: the reader sees EOF only after every process holding a write descriptor has closed it.

## Hint 3 — Parent event loop

Use `poll` with a short wait and `waitpid(..., WNOHANG)` to make progress on both output and child lifecycle. Retry operations interrupted by `EINTR`. When the deadline expires, send `SIGKILL` and reap the child with `waitpid`. Keep reading and discarding bytes after the capture limit so the child cannot block on a full pipe.

For queue shutdown, notify both condition variables after setting the closed flag while holding the mutex.
