#include "cppapplab/workspace.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace cppapplab {
namespace {

constexpr std::uintmax_t kMaxSourceBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaxEvidenceBytes = 16U * 1024U;
constexpr std::size_t kMaxLogBytes = 256U * 1024U;
constexpr const char* kWorkspaceMarker = "CppAppLab Workspace v1";

std::string read_regular_file(const std::filesystem::path& path, std::uintmax_t max_bytes) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect file: " + path.string() + ": " + error.message());
    }
    if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
        throw std::runtime_error("Expected a regular, non-symlink file: " + path.string());
    }

    const auto size = std::filesystem::file_size(path);
    if (size > max_bytes) {
        throw std::runtime_error("File exceeds the allowed size: " + path.string());
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed while reading file: " + path.string());
    }
    return content;
}

void atomic_write(const std::filesystem::path& path, const std::string& content) {
    if (std::filesystem::is_symlink(path)) {
        throw std::runtime_error("Refusing to replace a symlink: " + path.string());
    }

    auto temporary = path;
    temporary += ".tmp";
    if (std::filesystem::is_symlink(temporary)) {
        throw std::runtime_error("Refusing to write through a symlink: " + temporary.string());
    }

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Cannot create file: " + temporary.string());
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output) {
            throw std::runtime_error("Failed to write file: " + temporary.string());
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot publish file " + path.string() + ": " + error.message());
    }
}

void copy_tree_without_symlinks(const std::filesystem::path& source,
                                const std::filesystem::path& destination,
                                bool create_destination = true) {
    std::error_code error;
    const auto source_status = std::filesystem::symlink_status(source, error);
    if (error || !std::filesystem::is_directory(source_status) ||
        std::filesystem::is_symlink(source_status)) {
        throw std::runtime_error("Learner template is not a regular directory: " + source.string());
    }

    if (create_destination && !std::filesystem::create_directory(destination)) {
        throw std::runtime_error("Destination directory already exists: " + destination.string());
    }
    for (const auto& entry : std::filesystem::directory_iterator(source)) {
        const auto source_path = entry.path();
        const auto destination_path = destination / source_path.filename();
        const auto status = std::filesystem::symlink_status(source_path);

        if (std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Template contains a symlink: " + source_path.string());
        }
        if (std::filesystem::is_directory(status)) {
            copy_tree_without_symlinks(source_path, destination_path);
        } else if (std::filesystem::is_regular_file(status)) {
            std::filesystem::copy_file(source_path, destination_path);
        } else {
            throw std::runtime_error("Template contains an unsupported file type: " +
                                     source_path.string());
        }
    }
}

std::string fnv_hex(std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

void fnv_update(std::uint64_t& hash, const char* data, std::size_t size) {
    constexpr std::uint64_t prime = 1099511628211ULL;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= static_cast<unsigned char>(data[index]);
        hash *= prime;
    }
}

std::map<std::string, std::string> parse_key_values(const std::string& content) {
    std::map<std::string, std::string> values;
    std::istringstream input(content);
    std::string line;

    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0) {
            continue;
        }
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return values;
}

void validate_relative_source_path(const std::filesystem::path& relative_path,
                                   const std::filesystem::path& workspace_root) {
    if (relative_path.empty() || relative_path.is_absolute()) {
        throw std::runtime_error("Task source path must be relative to the workspace.");
    }

    std::filesystem::path current = workspace_root;
    std::size_t component_index = 0;
    const std::size_t component_count =
        static_cast<std::size_t>(std::distance(relative_path.begin(), relative_path.end()));

    for (const auto& component : relative_path) {
        if (component == "." || component == "..") {
            throw std::runtime_error("Task source path contains an unsafe component.");
        }

        current /= component;
        const auto status = std::filesystem::symlink_status(current);
        if (std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Learner source path contains a symlink: " + current.string());
        }

        ++component_index;
        if (component_index < component_count && !std::filesystem::is_directory(status)) {
            throw std::runtime_error("Learner source parent is not a directory: " +
                                     current.string());
        }
    }
}

unsigned parse_unsigned(const std::map<std::string, std::string>& values, const std::string& key) {
    const auto found = values.find(key);
    if (found == values.end()) {
        throw std::runtime_error("Evidence is missing field: " + key);
    }

    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(found->second, &consumed);
    if (consumed != found->second.size()) {
        throw std::runtime_error("Evidence contains an invalid number: " + key);
    }
    if (parsed > std::numeric_limits<unsigned>::max()) {
        throw std::runtime_error("Evidence number is out of range: " + key);
    }
    return static_cast<unsigned>(parsed);
}

}  // namespace

Workspace create_workspace(const Task& task, const std::filesystem::path& destination) {
    const auto absolute_destination = std::filesystem::absolute(destination).lexically_normal();

    if (std::filesystem::exists(absolute_destination) ||
        std::filesystem::is_symlink(absolute_destination)) {
        throw std::runtime_error("Workspace already exists; start never overwrites learner work: " +
                                 absolute_destination.string());
    }

    const auto template_path = data_root() / task.learner_template;
    std::filesystem::create_directories(absolute_destination.parent_path());
    if (!std::filesystem::create_directory(absolute_destination)) {
        throw std::runtime_error("Workspace already exists; start never overwrites learner work: " +
                                 absolute_destination.string());
    }

    try {
        copy_tree_without_symlinks(template_path, absolute_destination, false);

        const std::string marker = std::string(kWorkspaceMarker) + "\ntask_id=" + task.id + "\n";
        atomic_write(absolute_destination / ".cppapplab-workspace", marker);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(absolute_destination, ignored);
        throw;
    }

    return open_workspace(absolute_destination);
}

Workspace open_workspace(const std::filesystem::path& root) {
    const auto absolute_root = std::filesystem::absolute(root).lexically_normal();
    std::error_code error;
    const auto root_status = std::filesystem::symlink_status(absolute_root, error);
    if (error || !std::filesystem::is_directory(root_status) ||
        std::filesystem::is_symlink(root_status)) {
        throw std::runtime_error("Workspace must be a real directory: " + absolute_root.string());
    }

    const std::string marker = read_regular_file(absolute_root / ".cppapplab-workspace", 512U);
    std::istringstream input(marker);
    std::string header;
    std::string task_line;
    std::string extra;

    if (!std::getline(input, header) || header != kWorkspaceMarker ||
        !std::getline(input, task_line) || std::getline(input, extra)) {
        throw std::runtime_error("Workspace marker is malformed; no files were changed.");
    }

    constexpr std::string_view prefix = "task_id=";
    if (task_line.compare(0, prefix.size(), prefix) != 0) {
        throw std::runtime_error("Workspace marker does not contain a task ID.");
    }

    const Task* task = find_task(task_line.substr(prefix.size()));
    if (task == nullptr) {
        throw std::runtime_error("Workspace refers to an unknown task ID.");
    }

    return Workspace{absolute_root, task};
}

std::string source_fingerprint(const Task& task, const std::filesystem::path& workspace_root) {
    std::uint64_t hash = 14695981039346656037ULL;

    for (const auto& relative_path : task.source_files) {
        validate_relative_source_path(relative_path, workspace_root);
        const auto path = workspace_root / relative_path;
        const auto status = std::filesystem::symlink_status(path);

        if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Expected learner source file: " + path.string());
        }

        const auto size = std::filesystem::file_size(path);
        if (size > kMaxSourceBytes) {
            throw std::runtime_error("Learner source exceeds 1 MiB: " + path.string());
        }

        const std::string relative_name = relative_path.generic_string();
        fnv_update(hash, relative_name.data(), relative_name.size());
        const char separator = '\0';
        fnv_update(hash, &separator, 1);

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot read learner source: " + path.string());
        }

        char buffer[8192];
        while (input) {
            input.read(buffer, sizeof(buffer));
            const std::streamsize count = input.gcount();
            if (count > 0) {
                fnv_update(hash, buffer, static_cast<std::size_t>(count));
            }
        }
        if (!input.eof()) {
            throw std::runtime_error("Failed while reading learner source: " + path.string());
        }
    }

    return "fnv1a64:" + fnv_hex(hash);
}

std::optional<Evidence> read_evidence(const std::filesystem::path& workspace_root) {
    const auto state_directory = workspace_root / ".cppapplab";
    if (!std::filesystem::exists(state_directory) &&
        !std::filesystem::is_symlink(state_directory)) {
        return std::nullopt;
    }

    const auto directory_status = std::filesystem::symlink_status(state_directory);
    if (!std::filesystem::is_directory(directory_status) ||
        std::filesystem::is_symlink(directory_status)) {
        throw std::runtime_error("Workspace state path is not a regular directory.");
    }

    const auto evidence_path = state_directory / "evidence.txt";
    if (!std::filesystem::exists(evidence_path) && !std::filesystem::is_symlink(evidence_path)) {
        return std::nullopt;
    }

    const auto values = parse_key_values(read_regular_file(evidence_path, kMaxEvidenceBytes));
    const auto task = values.find("task_id");
    const auto fingerprint = values.find("fingerprint");
    const auto result = values.find("result");
    const auto timestamp = values.find("timestamp");

    if (task == values.end() || fingerprint == values.end() || result == values.end() ||
        timestamp == values.end()) {
        throw std::runtime_error("Evidence file is incomplete.");
    }
    if (result->second != "PASS" && result->second != "FAIL") {
        throw std::runtime_error("Evidence result must be PASS or FAIL.");
    }

    Evidence evidence;
    evidence.task_id = task->second;
    evidence.fingerprint = fingerprint->second;
    evidence.passed = result->second == "PASS";
    evidence.tests_passed = parse_unsigned(values, "tests_passed");
    evidence.tests_total = parse_unsigned(values, "tests_total");
    evidence.timestamp = timestamp->second;
    return evidence;
}

void write_evidence(const std::filesystem::path& workspace_root, const Evidence& evidence,
                    const std::string& log) {
    const auto state_directory = workspace_root / ".cppapplab";
    if (std::filesystem::exists(state_directory) || std::filesystem::is_symlink(state_directory)) {
        const auto status = std::filesystem::symlink_status(state_directory);
        if (!std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) {
            throw std::runtime_error(
                "Refusing to write through a non-directory workspace state path.");
        }
    } else {
        std::filesystem::create_directory(state_directory);
    }

    std::ostringstream content;
    content << "format=1\n"
            << "task_id=" << evidence.task_id << '\n'
            << "fingerprint=" << evidence.fingerprint << '\n'
            << "result=" << (evidence.passed ? "PASS" : "FAIL") << '\n'
            << "tests_passed=" << evidence.tests_passed << '\n'
            << "tests_total=" << evidence.tests_total << '\n'
            << "timestamp=" << evidence.timestamp << '\n';

    atomic_write(state_directory / "evidence.txt", content.str());

    const std::string bounded_log = log.size() > kMaxLogBytes ? log.substr(0, kMaxLogBytes) : log;
    atomic_write(state_directory / "last-check.log", bounded_log);
}

}  // namespace cppapplab
