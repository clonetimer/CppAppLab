#include "task.hpp"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace cppapplab::tasks {

Task::Task(std::uint64_t id, std::string title, bool completed)
    : id_(id), title_(std::move(title)), completed_(completed) {
    // TODO(W3-TASK-1): enforce the Task invariants at construction.
    (void)id_;
    (void)title_;
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
    // TODO(W3-TASK-2): update the completion state.
}

bool operator==(const Task& left, const Task& right) noexcept {
    // TODO(W3-TASK-3): define Task identity.
    (void)left;
    (void)right;
    return false;
}

bool operator<(const Task& left, const Task& right) noexcept {
    // TODO(W3-TASK-4): define a stable ordering by task ID.
    (void)left;
    (void)right;
    return false;
}

std::ostream& operator<<(std::ostream& output, const Task& task) {
    // TODO(W3-TASK-5): format a task as #id [x| ] title.
    (void)task;
    return output;
}

}  // namespace cppapplab::tasks
