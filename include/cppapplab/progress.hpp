#pragma once

#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppapplab {

struct ProgressRecord {
    Evidence evidence;
    std::filesystem::path workspace_root;
    bool checked{true};
};

[[nodiscard]] std::optional<ProgressRecord> read_progress(std::string_view task_id);

[[nodiscard]] std::vector<ProgressRecord> read_progress_history(std::string_view task_id);

void register_workspace(const std::filesystem::path& workspace_root);

void write_progress(const std::filesystem::path& workspace_root, const Evidence& evidence);

[[nodiscard]] bool has_current_pass(std::string_view task_id);

[[nodiscard]] bool prerequisites_satisfied(const Task& task,
                                           std::string* missing_prerequisite = nullptr);

[[nodiscard]] std::filesystem::path progress_directory();

}  // namespace cppapplab
