#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace cppapplab::tasks {

class Task {
public:
    Task(std::uint64_t id, std::string title, bool completed = false);

    Task(const Task&) = default;
    Task(Task&&) noexcept = default;
    Task& operator=(const Task&) = default;
    Task& operator=(Task&&) noexcept = default;
    ~Task() = default;

    [[nodiscard]] std::uint64_t id() const noexcept;
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] bool completed() const noexcept;

    void mark_done() noexcept;

    friend bool operator==(const Task& left, const Task& right) noexcept;
    friend bool operator<(const Task& left, const Task& right) noexcept;
    friend std::ostream& operator<<(std::ostream& output, const Task& task);

private:
    std::uint64_t id_{};
    std::string title_;
    bool completed_{false};
};

}  // namespace cppapplab::tasks
