#include "thread_pool.hpp"

#include <stdexcept>

namespace cppapplab::concurrency {

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t queue_capacity)
    : queue_(queue_capacity) {
    if (worker_count == 0U) {
        throw std::invalid_argument("thread pool requires at least one worker");
    }
    if (queue_capacity == 0U) {
        throw std::invalid_argument("thread pool queue capacity must be positive");
    }

    workers_.reserve(worker_count);
    try {
        for (std::size_t index = 0; index < worker_count; ++index) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    } catch (...) {
        queue_.close();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::worker_loop() {
    while (auto task = queue_.pop()) {
        try {
            (*task)();
        } catch (...) {
            // packaged_task stores user exceptions in its future. This catch is
            // a final worker-boundary guard for any unexpected wrapper failure.
        }
    }
}

void ThreadPool::shutdown() {
    std::call_once(shutdown_once_, [this] {
        {
            std::lock_guard<std::mutex> lock(lifecycle_mutex_);
            shutdown_started_.store(true);
            queue_.close();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    });
}

std::size_t ThreadPool::worker_count() const noexcept {
    return workers_.size();
}

std::size_t ThreadPool::pending() const {
    return queue_.size();
}

bool ThreadPool::is_shutdown() const noexcept {
    return shutdown_started_.load();
}

}  // namespace cppapplab::concurrency
