## Hint 1 — packaged_task and future

Use `std::invoke_result_t` to determine the return type. Capture the callable and a `std::tuple` of decayed arguments by move. Wrap the packaged task in `std::shared_ptr` so the queue can hold a copyable `std::function<void()>` closure. Obtain the future before enqueueing.

## Hint 2 — submit/shutdown ordering

Use one lifecycle mutex. Submission takes the mutex, checks the shutdown flag, then enqueues. Shutdown takes the same mutex, marks shutdown started and closes the queue. This makes the acceptance decision and queue close mutually ordered.

The submit operation may wait for queue capacity while holding the lifecycle mutex. Workers must not need that mutex to pop or execute tasks; otherwise the pool can deadlock.

## Hint 3 — concurrent shutdown

Use `std::call_once` around the close-and-join operation. Concurrent callers of `call_once` wait for the active invocation to finish. Do not allow two callers to call `join` on the same worker thread concurrently.

Worker loop should continue until `pop()` returns empty after close and drain. Execute each task outside the queue's internal mutex.
