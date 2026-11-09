#include "task.hpp"

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace cppapplab::tasks {
namespace {

constexpr std::size_t kMaxTitleBytes = 4096U;

bool valid_title(std::string_view title) {
    if (title.empty() || title.size() > kMaxTitleBytes) {
        return false;
    }
    return std::any_of(title.begin(), title.end(), [](unsigned char character) {
        return character != ' ' && character != '\t' &&
               character != '\r' && character != '\n';
    });
}

}  // namespace

Task::Task(std::uint64_t id, std::string title, bool completed)
    : id_(id), title_(std::move(title)), completed_(completed) {
    if (id_ == 0U) {
        throw std::invalid_argument("task ID must be positive");
    }
    if (!valid_title(title_)) {
        throw std::invalid_argument("task title cannot be empty or whitespace-only");
    }
}

std::uint64_t Task::id() const noexcept {
    return id_;
}

const std::string& Task::title() const noexcept {
    return title_;
}

bool Task::completed() const noexcept {
    return completed_;
}

void Task::mark_done() noexcept {
    completed_ = true;
}

bool operator==(const Task& left, const Task& right) noexcept {
    return left.id_ == right.id_;
}

bool operator<(const Task& left, const Task& right) noexcept {
    return left.id_ < right.id_;
}

std::ostream& operator<<(std::ostream& output, const Task& task) {
    output << '#' << task.id_ << (task.completed_ ? " [x] " : " [ ] ") << task.title_;
    return output;
}

}  // namespace cppapplab::tasks
