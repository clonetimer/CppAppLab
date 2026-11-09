#pragma once

#include "cppapplab/task.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace cppapplab {

struct Workspace {
    std::filesystem::path root;
    const Task* task{};
};

struct Evidence {
    std::string task_id;
    std::string fingerprint;
    bool passed{false};
    unsigned tests_passed{};
    unsigned tests_total{};
    std::string timestamp;
};

[[nodiscard]] Workspace create_workspace(const Task& task,
                                         const std::filesystem::path& destination);

[[nodiscard]] Workspace open_workspace(const std::filesystem::path& root);

[[nodiscard]] std::string source_fingerprint(const Task& task,
                                             const std::filesystem::path& workspace_root);

[[nodiscard]] std::optional<Evidence> read_evidence(const std::filesystem::path& workspace_root);

void write_evidence(const std::filesystem::path& workspace_root, const Evidence& evidence,
                    const std::string& log);

}  // namespace cppapplab
