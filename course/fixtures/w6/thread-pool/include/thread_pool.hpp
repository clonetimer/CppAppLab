#pragma once

#include "bounded_queue.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppapplab::concurrency {

class ThreadPool {
   public:
    explicit ThreadPool(std::size_t worker_count, std::size_t queue_capacity);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool();

    template <typename Function, typename... Args>
    auto submit(Function&& function,
                Args&&... args) -> std::future<std::invoke_result_t<Function, Args...>>;

    void shutdown();
    [[nodiscard]] std::size_t worker_count() const noexcept;
    [[nodiscard]] std::size_t pending() const;
    [[nodiscard]] bool is_shutdown() const noexcept;

   private:
    void worker_loop();

    system_lab::BoundedQueue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    mutable std::mutex lifecycle_mutex_;
    std::once_flag shutdown_once_;
    std::atomic<bool> shutdown_started_{false};
};

}  // namespace cppapplab::concurrency
