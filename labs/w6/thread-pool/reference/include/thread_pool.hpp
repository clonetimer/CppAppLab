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
                Args&&... args) -> std::future<std::invoke_result_t<Function, Args...>> {
        using ReturnType = std::invoke_result_t<Function, Args...>;
        auto bound_arguments = std::make_tuple(std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [callable = std::forward<Function>(function),
             arguments = std::move(bound_arguments)]() mutable -> ReturnType {
                return std::apply(
                    [&callable](auto&&... values) -> ReturnType {
                        return std::invoke(std::move(callable),
                                           std::forward<decltype(values)>(values)...);
                    },
                    std::move(arguments));
            });
        auto result = task->get_future();

        // Holding the lifecycle lock makes submission and shutdown linearizable.
        // A blocked producer is released when workers make room or shutdown closes
        // the queue; shutdown waits for this submission to finish before closing.
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (shutdown_started_.load()) {
            throw std::runtime_error("thread pool is shutting down");
        }
        if (!queue_.push([task] { (*task)(); })) {
            throw std::runtime_error("thread pool is shutting down");
        }
        return result;
    }

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
