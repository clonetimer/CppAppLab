# W6 · Thread Pool, Futures, and Graceful Shutdown

## Goal

Build a reusable fixed-size C++17 thread pool on top of the W5 bounded queue. The task focuses on asynchronous result delivery, callable/argument forwarding, exception propagation, backpressure, and concurrent shutdown semantics.

## Deliverables

Implement:

- `include/thread_pool.hpp` — the ThreadPool API and templated `submit`
- `src/thread_pool.cpp` — worker lifecycle and shutdown
- preserve the supplied CLI in `src/main.cpp`
- complete the ten learner tests in `tests/thread_pool_test.cpp`

The W5 `BoundedQueue<std::function<void()>>` is supplied as a reusable dependency. Do not copy or replace its synchronization implementation.

## API and semantics

The pool is constructed with a positive worker count and positive queue capacity.

`submit(function, args...)`:

- accepts arbitrary callables and arguments
- supports move-only callables and move-only arguments
- returns `std::future<R>` where R is the callable's result type
- stores task exceptions in the future; calling `get()` rethrows them
- applies bounded-queue backpressure while the queue is full
- rejects submission after shutdown has begun

Shutdown:

- is idempotent and safe when called by multiple external threads
- establishes a clear ordering between concurrent submit and shutdown calls
- closes the task queue and wakes blocked submitters
- drains tasks already accepted into the queue
- joins every worker before returning
- rejects submissions after shutdown

The destructor invokes shutdown. Do not detach worker threads.

## Critical concurrency contract

A task is accepted only if its enqueue operation completes before shutdown closes the queue. A task rejected by shutdown must not be silently discarded as accepted work.

The implementation must define a linearization point for submission versus shutdown. Protect the state check and enqueue operation with a lifecycle mutex; shutdown takes the same mutex before marking the pool closed and closing the queue. Be aware that bounded enqueue may block while holding this mutex: worker threads must continue draining accepted work so blocked submitters can progress, and shutdown must not close the queue in the middle of an in-flight submission.

Multiple shutdown callers must not race while joining the same std::thread objects. Use a once-only shutdown mechanism or an equivalent state machine that makes every caller wait until joining is complete.

Tasks should execute outside the queue lock. User exceptions must not terminate worker threads; packaged_task captures them for the associated future.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/thread-pool
```

## What the platform checks

Run:

```bash
./build/cppapplab check "/absolute/path/to/your/thread-pool-workspace"
```

The fixed fixture runs 23 platform-owned tests, ten learner tests and one CLI smoke test: 34 checks total.

Coverage includes argument validation, void/value returns, move-only inputs, exception propagation, multiple workers, exactly-once task execution, backpressure, queue draining, concurrent shutdown callers, submit/shutdown races, and destructor joining.

## Explain your design

1. Why does `std::packaged_task` make the callable wrapper copyable when stored in a queue of `std::function<void()>`?
2. What is the linearization point between `submit` and `shutdown`?
3. Why must multiple shutdown callers wait for the same join operation to finish?
4. How does a bounded queue apply backpressure, and what can deadlock if workers stop consuming too early?
5. Why should task exceptions be transported through `std::future` rather than escape a worker function?
6. What happens to a future if submission is rejected before the task is accepted?

## Acceptance

- [ ] Positive worker count and queue capacity are enforced.
- [ ] Callable and arguments support move-only types.
- [ ] Future returns values and void results correctly.
- [ ] Exceptions propagate through future::get.
- [ ] Multiple workers execute tasks concurrently.
- [ ] Bounded submission applies backpressure.
- [ ] Shutdown drains accepted tasks and rejects later submissions.
- [ ] Concurrent shutdown callers wait until all workers are joined.
- [ ] submit racing shutdown results in accepted-and-completed or rejected work, never silent loss.
- [ ] Destructor joins all workers.
- [ ] All 34 fixed checks pass.
