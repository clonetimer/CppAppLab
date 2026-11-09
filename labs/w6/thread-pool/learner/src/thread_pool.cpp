#include "thread_pool.hpp"

#include <stdexcept>

namespace cppapplab::concurrency {

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t queue_capacity)
    : queue_(queue_capacity) {
    // TODO(W6-POOL-3): validate arguments and start the requested worker threads.
    (void)worker_count;
}

ThreadPool::~ThreadPool() {
    // TODO(W6-POOL-4): ensure shutdown and join every worker.
}

void ThreadPool::worker_loop() {
    // TODO(W6-POOL-5): consume tasks until the queue is closed and drained.
}

void ThreadPool::shutdown() {
    // TODO(W6-POOL-6): serialize shutdown, close the queue and join workers exactly once.
}

std::size_t ThreadPool::worker_count() const noexcept { return workers_.size(); }
std::size_t ThreadPool::pending() const { return queue_.size(); }
bool ThreadPool::is_shutdown() const noexcept { return shutdown_started_.load(); }

}  // namespace cppapplab::concurrency
