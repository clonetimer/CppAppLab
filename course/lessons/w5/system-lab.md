# W5 · Linux System Programming Lab

## Start here

Begin with one process, not the whole Linux API surface.

1. Read the fixed CLI in `src/main.cpp` and identify the command that starts a child process.
2. Trace the data path: command arguments → `run_process` → child process → captured output and exit status.
3. Exercise the supplied bounded queue first: push one item, then pop it.
4. Run the focused queue test before adding concurrent producers and consumers.
5. Implement process execution with a fixed argument vector; then add timeout, output limit and child cleanup.
6. Finish by testing invalid input, timeout and output truncation.

**Mental model:** a file descriptor is a small integer handle owned by the process. `fork` creates a child; `exec` replaces its program; `waitpid` collects its exit status. Start with one child and follow its lifetime before studying pipes and process groups.


## Goal

Build a small Linux command runner backed by two reusable components:

1. a bounded, thread-safe producer/consumer queue
2. a POSIX child-process runner that captures output and reports process termination

This week connects the C++ concurrency library to real Linux kernel interfaces. The acceptance environments are WSL2 + Ubuntu 22.04 or 24.04 + GCC; this task is intentionally POSIX/Linux-specific.

## Your implementation

Implement descriptor I/O and child process management in `src/process_runner.cpp`, and complete the ten TODO tests in `tests/system_lab_test.cpp`.

The bounded queue template in `include/bounded_queue.hpp` is supplied. Read its implementation and exercise its producer/consumer behavior in your tests.

The CLI in `src/main.cpp` is supplied. The public API is declared in `include/process_runner.hpp`.

## Part A — bounded queue

The supplied queue uses a fixed capacity, a mutex, two condition variables, and a deque.

Required semantics:

- `push(value)` waits while the queue is full.
- `pop()` waits while the queue is empty.
- `close()` is idempotent and wakes all blocked producers and consumers.
- Closing rejects new pushes.
- Items already queued remain available after close.
- Once closed and drained, `pop()` returns `std::nullopt`.
- No busy-wait loops; use predicate overloads of condition-variable waits.
- All shared state is protected by the same mutex.

A zero-capacity queue is valid as an object but cannot accept or return items.

## Part B — POSIX child process runner

Implement `write_all` and `run_process` with Linux/POSIX APIs.

The process runner must:

- validate its argument vector, timeout and output limit
- create a pipe and fork a child
- close unused pipe descriptors in both processes
- redirect both child stdout and stderr into the pipe
- call `execvp` with an argument vector; do not build a shell command string
- read output in the parent and retry interrupted system calls
- cap captured output while continuing to drain the pipe
- use `waitpid` to collect the child's termination status
- kill and reap the child when the deadline expires
- distinguish normal exit from signal termination

The output cap limits memory usage; it must not cause the parent to stop draining the pipe and deadlock a child that is still writing.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure

./build/system-lab /bin/echo "hello from child"
./build/system-lab /bin/sh -c 'exit 6'
```

The second example returns the child's exit status. The process runner itself does not invoke a shell; the shell in that example is an explicit child program selected by the caller.

## Concurrency model

Use predicate waits rather than checking queue state in a loop without a condition variable. The predicates are:

- producer may proceed when the queue is closed or has free capacity
- consumer may proceed when the queue is closed or contains an item

The close operation must notify both condition variables. This prevents a blocked producer or consumer from remaining asleep after shutdown.

## What the platform checks

Run:

```bash
./build/cppapplab check "/absolute/path/to/your/system-lab-workspace"
```

The fixed fixture runs 20 platform-owned tests, 10 learner-authored tests, and one CLI smoke test: 31 checks total.

Coverage includes queue FIFO/capacity/close semantics, blocked-thread wakeups, multiple producers and consumers, POSIX output capture, stderr capture, exit status, argument-vector safety, invalid arguments, bounded output, timeout termination, and descriptor write completion.

The platform uses its own headers and CMake fixture. Learner-controlled CMake files are not used as the acceptance authority.

## Explain your design

1. Why must every process close pipe ends it does not use?
2. Why does the parent need to drain the pipe even after reaching the output limit?
3. Why is an argv vector safer and more composable than constructing a shell command string?
4. Why should condition-variable waits use predicates?
5. What must happen to blocked producers and consumers when the queue is closed?
6. Why must the parent call waitpid after killing a timed-out child?

## Troubleshooting

- **A queue test waits forever.** Check whether it pushes into a full queue or pops an empty one before another thread can make progress. Arrange a producer/consumer to release the wait; during shutdown, call `close` and verify both kinds of waiter wake while queued items remain drainable.
- **The echo command produces no output and exits unsuccessfully.** Inspect `run_process` in `src/process_runner.cpp`: the starter returns an empty `ProcessResult`. Trace one `/bin/echo` child through execution, captured output and collected exit status before adding timeout cases.
- **The child exits but the parent never sees pipe EOF.** Inspect every pipe descriptor inherited across `fork`, especially the parent's copy of the write end. Close unused ends in both processes, then finish draining the pipe and collect the child with `waitpid`.
- **Large output or a timeout leaves the check stuck.** Inspect the output-cap and deadline paths separately. Reaching the cap must stop storing bytes without stopping reads; an expired deadline must terminate and reap the child. Retry the bounded-output and timeout cases after each path works independently.

## Acceptance

- [ ] Queue push/pop are bounded and thread-safe.
- [ ] Close wakes blocked producers and consumers.
- [ ] Existing items can be drained after close.
- [ ] Concurrent producers/consumers do not lose or duplicate items.
- [ ] write_all handles partial writes and EINTR.
- [ ] Process runner uses pipe/fork/dup2/execvp/waitpid.
- [ ] stdout and stderr are captured.
- [ ] Child exit status and signal termination are reported.
- [ ] Timeout kills and reaps the child.
- [ ] Output capture is bounded without stopping pipe draining.
- [ ] All 31 fixed checks pass.
