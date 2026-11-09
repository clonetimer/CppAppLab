#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cppapplab {

enum class AcceptanceKind {
    ExactStdout
};

struct Task {
    std::string id;
    std::string title;
    unsigned week{};
    unsigned order{};
    std::filesystem::path lesson_path;
    std::filesystem::path hints_path;
    std::filesystem::path learner_template;
    std::vector<std::filesystem::path> source_files;
    AcceptanceKind acceptance_kind{AcceptanceKind::ExactStdout};
    std::string expected_stdout;
};

[[nodiscard]] const std::vector<Task>& task_catalog();
[[nodiscard]] const Task* find_task(std::string_view id);
[[nodiscard]] std::filesystem::path data_root();

}  // namespace cppapplab
