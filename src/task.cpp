#include "cppapplab/task.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef CPPAPPLAB_DATA_DIR
#define CPPAPPLAB_DATA_DIR "."
#endif

namespace cppapplab {
namespace {

constexpr std::size_t kMaxCatalogBytes = 64U * 1024U;
constexpr std::size_t kCatalogFieldCount = 13U;

bool valid_task_id(std::string_view task_id) {
    if (task_id.empty()) {
        return false;
    }
    for (const char character : task_id) {
        const bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9') ||
            character == '.' || character == '-' || character == '_';
        if (!valid) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split_fields(const std::string& line, char separator) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t position = line.find(separator, start);
        if (position == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, position - start));
        start = position + 1;
    }
    return fields;
}

std::vector<std::filesystem::path> parse_paths(const std::string& value) {
    std::vector<std::filesystem::path> paths;
    if (value.empty()) {
        return paths;
    }

    for (const std::string& item : split_fields(value, ';')) {
        const std::filesystem::path path(item);
        if (path.empty() || path.is_absolute()) {
            throw std::runtime_error("Task catalog contains an invalid relative path.");
        }
        for (const auto& component : path) {
            if (component == "." || component == "..") {
                throw std::runtime_error("Task catalog paths cannot contain dot components.");
            }
        }
        paths.push_back(path);
    }
    return paths;
}

std::vector<std::string> parse_ids(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const auto ids = split_fields(value, ';');
    for (const std::string& id : ids) {
        if (!valid_task_id(id)) {
            throw std::runtime_error("Task catalog contains an invalid prerequisite ID.");
        }
    }
    return ids;
}

unsigned parse_unsigned(const std::string& value, const std::string& field) {
    if (value.empty()) {
        throw std::runtime_error("Task catalog field is empty: " + field);
    }
    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(value, &consumed);
    if (consumed != value.size() || parsed > std::numeric_limits<unsigned>::max()) {
        throw std::runtime_error("Task catalog field is not a valid unsigned integer: " + field);
    }
    return static_cast<unsigned>(parsed);
}

std::string unescape_value(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\') {
            result.push_back(value[index]);
            continue;
        }
        if (++index >= value.size()) {
            throw std::runtime_error("Task catalog contains an incomplete escape sequence.");
        }
        switch (value[index]) {
            case 'n':
                result.push_back('\n');
                break;
            case 't':
                result.push_back('\t');
                break;
            case '\\':
                result.push_back('\\');
                break;
            default:
                throw std::runtime_error("Task catalog contains an unsupported escape sequence.");
        }
    }
    return result;
}

std::filesystem::path required_path(
    const std::string& value,
    const std::string& field
) {
    const auto paths = parse_paths(value);
    if (paths.size() != 1U) {
        throw std::runtime_error("Task catalog requires exactly one path for field: " + field);
    }
    return paths.front();
}

Task parse_task(const std::vector<std::string>& fields) {
    if (fields.size() != kCatalogFieldCount) {
        throw std::runtime_error("Task catalog row has an unexpected number of columns.");
    }

    Task task;
    task.id = fields[0];
    task.title = fields[1];
    task.week = parse_unsigned(fields[2], "week");
    task.order = parse_unsigned(fields[3], "order");
    task.lesson_path = required_path(fields[4], "lesson_path");
    task.hints_path = required_path(fields[5], "hints_path");
    task.learner_template = required_path(fields[6], "learner_template");
    task.source_files = parse_paths(fields[7]);

    if (fields[8] == "exact_stdout") {
        task.acceptance_kind = AcceptanceKind::ExactStdout;
    } else if (fields[8] == "cmake_gtest") {
        task.acceptance_kind = AcceptanceKind::CMakeGoogleTest;
    } else {
        throw std::runtime_error("Task catalog contains an unknown acceptance kind: " + fields[8]);
    }

    task.expected_stdout = unescape_value(fields[9]);
    if (!fields[10].empty()) {
        task.acceptance_fixture = required_path(fields[10], "acceptance_fixture");
    }
    task.expected_unit_test_count = fields[11].empty()
                                        ? 0U
                                        : parse_unsigned(fields[11], "expected_unit_test_count");
    task.prerequisites = parse_ids(fields[12]);

    if (!valid_task_id(task.id) || task.title.empty() ||
        task.week == 0U || task.order == 0U || task.source_files.empty()) {
        throw std::runtime_error("Task catalog row is missing or contains invalid task metadata.");
    }

    if (task.acceptance_kind == AcceptanceKind::ExactStdout) {
        if (task.expected_stdout.empty() || !task.acceptance_fixture.empty() ||
            task.expected_unit_test_count != 0U) {
            throw std::runtime_error("Exact-stdout task has inconsistent acceptance metadata.");
        }
    } else if (task.acceptance_fixture.empty() || task.expected_unit_test_count == 0U ||
               !task.expected_stdout.empty()) {
        throw std::runtime_error("CMake/GoogleTest task has inconsistent acceptance metadata.");
    }

    return task;
}

std::vector<Task> load_catalog() {
    const auto path = data_root() / "course" / "tasks.tsv";
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
        throw std::runtime_error(
            "Task catalog must be a regular non-symlink file: " + path.string()
        );
    }

    if (std::filesystem::file_size(path) > kMaxCatalogBytes) {
        throw std::runtime_error("Task catalog exceeds the 64 KiB size limit.");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open task catalog: " + path.string());
    }

    std::vector<Task> tasks;
    std::set<std::string> task_ids;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        Task task = parse_task(split_fields(line, '\t'));
        if (!task_ids.insert(task.id).second) {
            throw std::runtime_error("Task catalog contains a duplicate task ID: " + task.id);
        }
        tasks.push_back(std::move(task));
    }

    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed while reading the task catalog.");
    }
    if (tasks.empty()) {
        throw std::runtime_error("Task catalog does not contain any tasks.");
    }

    std::sort(tasks.begin(), tasks.end(), [](const Task& left, const Task& right) {
        if (left.week != right.week) {
            return left.week < right.week;
        }
        return left.order < right.order;
    });

    std::set<std::string> seen;
    for (const Task& task : tasks) {
        for (const std::string& prerequisite : task.prerequisites) {
            if (task_ids.find(prerequisite) == task_ids.end()) {
                throw std::runtime_error(
                    "Task prerequisite does not exist: " + task.id + " -> " + prerequisite
                );
            }
            if (seen.find(prerequisite) == seen.end()) {
                throw std::runtime_error(
                    "Task prerequisite must appear earlier in course order: " +
                    task.id + " -> " + prerequisite
                );
            }
        }
        seen.insert(task.id);
    }

    return tasks;
}

}  // namespace

std::filesystem::path data_root() {
    if (const char* override_path = std::getenv("CPPAPPLAB_DATA_DIR");
        override_path != nullptr && override_path[0] != '\0') {
        return std::filesystem::absolute(override_path).lexically_normal();
    }

    return std::filesystem::path(CPPAPPLAB_DATA_DIR).lexically_normal();
}

const std::vector<Task>& task_catalog() {
    static const std::vector<Task> tasks = load_catalog();
    return tasks;
}

const Task* find_task(std::string_view id) {
    for (const Task& task : task_catalog()) {
        if (task.id == id) {
            return &task;
        }
    }
    return nullptr;
}

}  // namespace cppapplab
