#pragma once

#include "task.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cppapplab::tasks {

enum class Error { None, InvalidId, InvalidTitle, DuplicateId, NotFound, InvalidFormat, IoError };

struct Result {
    Error error{Error::None};
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return error == Error::None;
    }

    explicit operator bool() const noexcept {
        return ok();
    }
};

class TaskManager {
   public:
    TaskManager() = default;
    TaskManager(const TaskManager& other);
    TaskManager& operator=(const TaskManager& other);
    TaskManager(TaskManager&& other) noexcept;
    TaskManager& operator=(TaskManager&& other) noexcept;
    ~TaskManager() = default;

    [[nodiscard]] Result add(std::uint64_t id, std::string_view title);
    [[nodiscard]] Result mark_done(std::uint64_t id);
    [[nodiscard]] Result remove(std::uint64_t id);

    [[nodiscard]] Task* find(std::uint64_t id) noexcept;
    [[nodiscard]] const Task* find(std::uint64_t id) const noexcept;

    [[nodiscard]] std::uint64_t next_id() const noexcept;
    [[nodiscard]] std::vector<Task> snapshot() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] Result save(const std::filesystem::path& path) const;
    [[nodiscard]] Result load(const std::filesystem::path& path);

    void swap(TaskManager& other) noexcept;

   private:
    std::vector<std::unique_ptr<Task>> tasks_;
};

}  // namespace cppapplab::tasks
