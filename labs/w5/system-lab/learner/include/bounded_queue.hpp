#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace cppapplab::system_lab {

template <typename T>
class BoundedQueue {
   public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    [[nodiscard]] bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        queue_.push_back(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    [[nodiscard]] std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

   private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    bool closed_{false};
};

}  // namespace cppapplab::system_lab
