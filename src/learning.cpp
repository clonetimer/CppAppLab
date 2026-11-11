#include "cppapplab/learning.hpp"

#include "cppapplab/app.hpp"
#include "cppapplab/process.hpp"
#include "cppapplab/progress.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <optional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace cppapplab {
namespace {

constexpr std::size_t kMaxLessonBytes = 64U * 1024U;
constexpr std::size_t kMaxHintBytes = 32U * 1024U;
constexpr std::size_t kMaxCompilerOutput = 128U * 1024U;
constexpr std::size_t kMaxProgramOutput = 16U * 1024U;

std::string read_lesson_file(const std::filesystem::path& path, std::size_t max_bytes) {
    const auto status = std::filesystem::symlink_status(path);
    if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
        throw std::runtime_error("Course material must be a regular file: " + path.string());
    }

    const auto size = std::filesystem::file_size(path);
    if (size > max_bytes) {
        throw std::runtime_error("Course material exceeds the configured size limit.");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open course material: " + path.string());
    }

    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed while reading course material.");
    }
    return content;
}

std::string extract_hint(const std::string& content, unsigned level) {
    const std::string heading = "## Hint " + std::to_string(level);
    const std::size_t heading_position = content.find(heading);
    if (heading_position == std::string::npos) {
        throw std::runtime_error("The requested hint level does not exist.");
    }

    const std::size_t body_start = content.find('\n', heading_position);
    if (body_start == std::string::npos) {
        return {};
    }

    const std::size_t next_heading = content.find("\n## Hint ", body_start + 1);
    const std::size_t body_end = next_heading == std::string::npos ? content.size() : next_heading;
    return content.substr(body_start + 1, body_end - body_start - 1);
}

void print_task_catalog() {
    std::cout << "CppAppLab · Available tasks\n\n";
    for (const Task& task : task_catalog()) {
        std::cout << task.id << "  |  " << task.title << "  |  W" << task.week;
        if (!task.prerequisites.empty()) {
            std::cout << "  |  requires ";
            for (std::size_t index = 0; index < task.prerequisites.size(); ++index) {
                if (index != 0U) {
                    std::cout << ", ";
                }
                std::cout << task.prerequisites[index];
            }
        }
        std::cout << '\n';
    }
    std::cout << "\nStart or resume the next task with: cppapplab learn\n"
              << "Choose a task explicitly with: cppapplab learn <task-id>\n";
}

void ensure_real_directory(const std::filesystem::path& path) {
    if (std::filesystem::exists(path) || std::filesystem::is_symlink(path)) {
        const auto status = std::filesystem::symlink_status(path);
        if (!std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Refusing to use a non-directory or symlink: " +
                                     path.string());
        }
    } else {
        std::filesystem::create_directory(path);
    }
}

std::vector<std::string> diagnostic_excerpt(const std::string& output) {
    std::vector<std::string> selected;
    std::vector<std::string> recent;
    std::istringstream input(output);
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        recent.push_back(line);
        if (recent.size() > 6U) {
            recent.erase(recent.begin());
        }

        const bool relevant =
            line.find("[CppAppLab]") != std::string::npos ||
            line.find("error") != std::string::npos || line.find("Error") != std::string::npos ||
            line.find("warning:") != std::string::npos ||
            line.find("FAILED") != std::string::npos || line.find("Failed") != std::string::npos ||
            line.find("failed") != std::string::npos ||
            line.find("undefined reference") != std::string::npos ||
            line.find("not found") != std::string::npos ||
            line.find("No such file") != std::string::npos ||
            line.find("Expected bytes") != std::string::npos ||
            line.find("Actual bytes") != std::string::npos;
        if (relevant && selected.size() < 8U) {
            if (line.size() > 220U) {
                line = line.substr(0U, 217U) + "...";
            }
            selected.push_back(line);
        }
    }

    return selected.empty() ? recent : selected;
}

void print_check_guidance(const Workspace& workspace, bool passed, const std::string& log_output) {
    if (passed) {
        std::cout << "Next step: ./build/cppapplab learn\n";
        return;
    }

    const auto diagnostics = diagnostic_excerpt(log_output);
    std::cout << "\nWhat needs attention:\n";
    if (diagnostics.empty()) {
        std::cout << "  The check did not pass. Open the full log for details.\n";
    } else {
        for (const auto& line : diagnostics) {
            std::cout << "  " << line << '\n';
        }
    }

    const auto log_path = workspace.root / ".cppapplab/last-check.log";
    std::cout << "\nSuggested next steps:\n"
              << "  1. Read the full log: cat \"" << log_path.string() << "\"\n"
              << "  2. Request a hint: ./build/cppapplab hint \"" << workspace.root.string()
              << "\" 1\n"
              << "  3. Fix the listed source files and rerun: ./build/cppapplab check \""
              << workspace.root.string() << "\"\n";
}

int check_exact_stdout_workspace(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    const Task& task = *workspace.task;

    if (task.acceptance_kind != AcceptanceKind::ExactStdout || task.source_files.size() != 1) {
        throw std::runtime_error("This task's acceptance runner is not implemented yet.");
    }

    const std::string fingerprint_before = source_fingerprint(task, workspace.root);

    const auto state_directory = workspace.root / ".cppapplab";
    ensure_real_directory(state_directory);

    const auto build_directory = state_directory / "build";
    ensure_real_directory(build_directory);

    const auto executable = build_directory / "learner_app";
    if (std::filesystem::exists(executable) || std::filesystem::is_symlink(executable)) {
        const auto status = std::filesystem::symlink_status(executable);
        if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Refusing to replace a non-regular build output.");
        }
        std::filesystem::remove(executable);
    }

    const auto source = workspace.root / task.source_files.front();
    const std::vector<std::string> compile_arguments{
        "g++",     "-std=c++17", "-Wall",         "-Wextra", "-Wpedantic",
        "-Werror", "-g",         source.string(), "-o",      executable.string()};

    std::ostringstream log;
    log << "$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -g "
        << task.source_files.front().generic_string() << " -o .cppapplab/build/learner_app\n";

    const ProcessResult compilation = run_process(compile_arguments, workspace.root,
                                                  std::chrono::seconds(30), kMaxCompilerOutput);
    log << compilation.output;

    bool passed = false;
    if (compilation.timed_out) {
        log << "\n[CppAppLab] Compilation exceeded the 30-second time limit.\n";
    } else if (compilation.output_truncated) {
        log << "\n[CppAppLab] Compiler output exceeded the capture limit.\n";
    } else if (compilation.exit_code != 0) {
        log << "\n[CppAppLab] Compilation failed with exit code " << compilation.exit_code << ".\n";
    } else {
        log << "\n$ .cppapplab/build/learner_app\n";
        const ProcessResult execution = run_process({executable.string()}, workspace.root,
                                                    std::chrono::seconds(5), kMaxProgramOutput);
        log << execution.output;

        if (execution.timed_out) {
            log << "\n[CppAppLab] Program exceeded the 5-second time limit.\n";
        } else if (execution.output_truncated) {
            log << "\n[CppAppLab] Program output exceeded the capture limit.\n";
        } else if (execution.exit_code != 0) {
            log << "\n[CppAppLab] Program exited with code " << execution.exit_code << ".\n";
        } else if (execution.output != task.expected_stdout) {
            log << "\n[CppAppLab] Output did not match the task contract.\n"
                << "Expected bytes: " << task.expected_stdout.size() << '\n'
                << "Actual bytes: " << execution.output.size() << '\n';
        } else {
            passed = true;
        }
    }

    const std::string fingerprint_after = source_fingerprint(task, workspace.root);
    if (fingerprint_after != fingerprint_before) {
        passed = false;
        log << "\n[CppAppLab] Learner source changed while the check was running; "
               "this result is not accepted.\n";
    }

    Evidence evidence;
    evidence.task_id = task.id;
    evidence.fingerprint = fingerprint_before;
    evidence.passed = passed;
    evidence.tests_passed = passed ? 1U : 0U;
    evidence.tests_total = 1U;

    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    if (::gmtime_r(&timestamp, &utc) != nullptr) {
        char formatted[32]{};
        if (std::strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &utc) != 0) {
            evidence.timestamp = formatted;
        }
    }
    if (evidence.timestamp.empty()) {
        evidence.timestamp = "unknown";
    }

    write_evidence(workspace.root, evidence, log.str());
    write_progress(workspace.root, evidence);

    std::cout << "Task: " << task.id << '\n'
              << "Result: " << (passed ? "PASS" : "FAIL") << '\n'
              << "Acceptance: " << evidence.tests_passed << '/' << evidence.tests_total << '\n'
              << "Source fingerprint: " << evidence.fingerprint << '\n'
              << "Evidence: " << (workspace.root / ".cppapplab/evidence.txt").string() << '\n'
              << "Log: " << (workspace.root / ".cppapplab/last-check.log").string() << '\n';

    print_check_guidance(workspace, passed, log.str());
    return passed ? 0 : 1;
}

std::filesystem::path stage_declared_cpp_sources(const Task& task,
                                                 const std::filesystem::path& workspace_root,
                                                 const std::filesystem::path& build_directory) {
    const auto stage_directory = build_directory / "learner-source";
    if (std::filesystem::exists(stage_directory) || std::filesystem::is_symlink(stage_directory)) {
        const auto status = std::filesystem::symlink_status(stage_directory);
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
            throw std::runtime_error("Refusing to replace an unsafe learner staging path.");
        }
        std::filesystem::remove_all(stage_directory);
    }
    std::filesystem::create_directories(stage_directory);

    std::size_t staged_count = 0;
    for (const auto& relative_path : task.source_files) {
        // Stage declared implementation and interface files only. In particular,
        // W6's templated submit() must compile from the learner's own header.
        // Platform-owned dependencies such as W5 BoundedQueue are deliberately
        // omitted from the W6 task catalog and remain supplied by the fixture.
        if (relative_path.extension() != ".cpp" && relative_path.extension() != ".hpp") {
            continue;
        }

        const auto source = workspace_root / relative_path;
        const auto destination = stage_directory / relative_path;
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(source, destination);
        ++staged_count;
    }

    if (staged_count == 0U) {
        throw std::runtime_error("Task has no declared C++ implementation files to stage.");
    }
    return stage_directory;
}

bool process_succeeded(const ProcessResult& result) {
    return !result.timed_out && !result.output_truncated && result.exit_code == 0;
}

int check_cmake_gtest_workspace(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    const Task& task = *workspace.task;
    if (task.acceptance_kind != AcceptanceKind::CMakeGoogleTest || task.expected_test_count == 0U) {
        throw std::runtime_error(
            "This task has no configured CMake/GoogleTest acceptance contract.");
    }

    const std::string fingerprint_before = source_fingerprint(task, workspace.root);
    const auto state_directory = workspace.root / ".cppapplab";
    ensure_real_directory(state_directory);
    const auto build_directory = state_directory / (task.id + "-build");
    ensure_real_directory(build_directory);

    std::ostringstream log;
    unsigned tests_passed = 0U;
    const unsigned tests_total = task.expected_test_count;
    bool passed = false;

    const auto fixture = data_root() / task.acceptance_fixture;
    const auto fixture_status = std::filesystem::symlink_status(fixture);
    if (!std::filesystem::is_directory(fixture_status) ||
        std::filesystem::is_symlink(fixture_status)) {
        throw std::runtime_error("Platform acceptance fixture is missing or unsafe.");
    }

    const auto staged_sources = stage_declared_cpp_sources(task, workspace.root, build_directory);

    const std::vector<std::string> configure_arguments{
        "cmake",
        "-S",
        fixture.string(),
        "-B",
        build_directory.string(),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCPPAPPLAB_LEARNER_SOURCE_DIR=" + staged_sources.string()};
    log << "$ cmake -S " << fixture.string() << " -B " << build_directory.string()
        << " -DCPPAPPLAB_LEARNER_SOURCE_DIR=" << staged_sources.string() << '\n';

    const ProcessResult configure = run_process(configure_arguments, workspace.root,
                                                std::chrono::seconds(30), kMaxCompilerOutput);
    log << configure.output;

    const bool configured = process_succeeded(configure);
    if (!configured) {
        log << "\n[CppAppLab] CMake configure failed or exceeded its resource limit.\n";
    }

    bool built = false;
    if (configured) {
        const ProcessResult build =
            run_process({"cmake", "--build", build_directory.string(), "--parallel", "2"},
                        workspace.root, std::chrono::seconds(90), kMaxCompilerOutput);
        log << "\n$ cmake --build " << build_directory.string() << " --parallel 2\n"
            << build.output;
        built = process_succeeded(build);
        if (!built) {
            log << "\n[CppAppLab] Platform-owned fixture build failed or exceeded its resource "
                   "limit.\n";
        }
    }

    bool acceptance_tests_passed = false;
    if (built) {
        const ProcessResult ctest =
            run_process({"ctest", "--test-dir", build_directory.string(), "--output-on-failure"},
                        workspace.root, std::chrono::seconds(45), kMaxCompilerOutput);
        log << "\n$ ctest --test-dir " << build_directory.string() << " --output-on-failure\n"
            << ctest.output;

        const std::string expected_summary =
            "100% tests passed, 0 tests failed out of " + std::to_string(task.expected_test_count);
        acceptance_tests_passed =
            process_succeeded(ctest) && ctest.output.find(expected_summary) != std::string::npos;

        if (acceptance_tests_passed) {
            tests_passed = task.expected_test_count;
        } else {
            log << "\n[CppAppLab] Fixed acceptance test count/result did not match the task "
                   "contract.\n";
        }
    }

    const std::string fingerprint_after = source_fingerprint(task, workspace.root);
    if (fingerprint_after != fingerprint_before) {
        log << "\n[CppAppLab] Learner source changed while the check was running; "
               "the result is not accepted.\n";
    } else {
        passed = configured && built && acceptance_tests_passed && tests_passed == tests_total;
    }

    Evidence evidence;
    evidence.task_id = task.id;
    evidence.fingerprint = fingerprint_before;
    evidence.passed = passed;
    evidence.tests_passed = tests_passed;
    evidence.tests_total = tests_total;

    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    if (::gmtime_r(&timestamp, &utc) != nullptr) {
        char formatted[32]{};
        if (std::strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &utc) != 0) {
            evidence.timestamp = formatted;
        }
    }
    if (evidence.timestamp.empty()) {
        evidence.timestamp = "unknown";
    }

    write_evidence(workspace.root, evidence, log.str());
    write_progress(workspace.root, evidence);

    std::cout << "Task: " << task.id << '\n'
              << "Result: " << (passed ? "PASS" : "FAIL") << '\n'
              << "Acceptance: " << evidence.tests_passed << '/' << evidence.tests_total << '\n'
              << "Source fingerprint: " << evidence.fingerprint << '\n'
              << "Evidence: " << (workspace.root / ".cppapplab/evidence.txt").string() << '\n'
              << "Log: " << (workspace.root / ".cppapplab/last-check.log").string() << '\n';

    print_check_guidance(workspace, passed, log.str());
    return passed ? 0 : 1;
}

int check_workspace(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    switch (workspace.task->acceptance_kind) {
        case AcceptanceKind::ExactStdout:
            return check_exact_stdout_workspace(workspace_path);
        case AcceptanceKind::CMakeGoogleTest:
            return check_cmake_gtest_workspace(workspace_path);
    }
    throw std::runtime_error("Unknown task acceptance kind.");
}

int show_status(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    const auto evidence = read_evidence(workspace.root);

    std::cout << "Workspace: " << workspace.root.string() << '\n'
              << "Task: " << workspace.task->id << '\n';

    if (!evidence.has_value()) {
        std::cout << "Status: NOT CHECKED\n"
                  << "Run: cppapplab check \"" << workspace.root.string() << "\"\n";
        return 0;
    }

    const std::string current_fingerprint = source_fingerprint(*workspace.task, workspace.root);

    if (evidence->task_id != workspace.task->id) {
        throw std::runtime_error("Evidence task ID does not match this workspace.");
    }

    if (evidence->fingerprint != current_fingerprint) {
        std::cout << "Last recorded result: STALE\n"
                  << "Previous result: " << (evidence->passed ? "PASS" : "FAIL") << '\n'
                  << "Previous source: " << evidence->fingerprint << '\n'
                  << "Current source:  " << current_fingerprint << '\n'
                  << "The source changed after the last check; rerun check.\n";
        return 0;
    }

    std::cout << "Last recorded result: " << (evidence->passed ? "PASS" : "FAIL") << '\n'
              << "Acceptance: " << evidence->tests_passed << '/' << evidence->tests_total << '\n'
              << "Checked at: " << evidence->timestamp << '\n'
              << "Source fingerprint: " << evidence->fingerprint << '\n'
              << "Note: local evidence is a navigation record, not a certificate; "
                 "rerun check to verify the current source.\n";
    return 0;
}

int show_next(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    if (!has_current_pass(workspace.task->id)) {
        std::cout << "Current task does not have a current PASS in course progress.\n"
                  << "Complete the task and run cppapplab check first.\n";
        return 1;
    }

    std::cout << "Current task has a current PASS: " << workspace.task->id << '\n';
    bool found_current = false;
    for (const Task& candidate : task_catalog()) {
        if (candidate.id == workspace.task->id) {
            found_current = true;
            continue;
        }
        if (!found_current) {
            continue;
        }

        std::string missing_prerequisite;
        if (prerequisites_satisfied(candidate, &missing_prerequisite)) {
            std::cout << "Next available task: " << candidate.id << " · " << candidate.title << '\n'
                      << "Continue with: cppapplab learn\n"
                      << "Or choose it explicitly: cppapplab learn " << candidate.id << '\n';
            std::cout << "Course progress is saved independently of this workspace.\n";
            return 0;
        }

        std::cout << "Next task is locked: " << candidate.id << " requires a current PASS for "
                  << missing_prerequisite << ".\n";
    }

    std::cout << "No later task is currently available.\n";
    return 0;
}

int show_progress() {
    std::cout << "CppAppLab · Course progress\n\n";
    for (const Task& task : task_catalog()) {
        const auto record = read_progress(task.id);
        std::string state;
        std::string detail;

        if (!record.has_value()) {
            std::string missing_prerequisite;
            if (prerequisites_satisfied(task, &missing_prerequisite)) {
                state = "NOT STARTED";
            } else {
                state = "LOCKED";
                detail = "requires " + missing_prerequisite;
            }
        } else if (has_current_pass(task.id)) {
            state = "PASSED";
            detail = record->checked ? record->evidence.timestamp
                                     : "another workspace has a current PASS";
        } else if (!record->checked) {
            state = "IN PROGRESS";
            detail = record->workspace_root.string();
        } else if (record->evidence.passed) {
            state = "STALE";
            detail = "source changed or saved workspace is unavailable";
        } else {
            state = "NEEDS WORK";
            detail = record->evidence.timestamp;
        }

        std::cout << '[' << state << "] " << task.id << " · " << task.title;
        if (!detail.empty()) {
            std::cout << "  (" << detail << ')';
        }
        std::cout << '\n';
    }
    std::cout << "\nProgress records are local navigation data, not certificates.\n";
    return 0;
}

int resume_task(std::string_view task_id) {
    const auto record = read_progress(task_id);
    if (!record.has_value()) {
        std::cout << "No saved workspace record exists for task " << task_id << ".\n";
        return 1;
    }

    try {
        const Workspace workspace = open_workspace(record->workspace_root);
        if (workspace.task->id != task_id) {
            throw std::runtime_error("Saved workspace belongs to a different task.");
        }
        std::cout << "Resuming task: " << workspace.task->id << '\n'
                  << "Workspace: " << workspace.root.string() << '\n';
        return show_status(workspace.root);
    } catch (const std::exception& error) {
        std::cout << "Saved workspace is unavailable: " << record->workspace_root.string() << '\n'
                  << "Reason: " << error.what() << '\n'
                  << "Create a new workspace with cppapplab start if the original was moved or "
                     "deleted.\n";
        return 2;
    }
}

std::filesystem::path default_workspace_root(const Task& task) {
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        throw std::runtime_error(
            "HOME is not set; use the explicit start command or set HOME before using learn.");
    }
    return std::filesystem::path(home) / "cppapplab-work" / task.id;
}

bool workspace_path_is_occupied(const std::filesystem::path& path) {
    return std::filesystem::exists(path) || std::filesystem::is_symlink(path);
}

std::optional<Workspace> find_existing_workspace(const Task& task) {
    const auto saved = read_progress(task.id);
    if (saved.has_value()) {
        try {
            const Workspace workspace = open_workspace(saved->workspace_root);
            if (workspace.task->id == task.id) {
                return workspace;
            }
        } catch (const std::exception& error) {
            std::cout << "Saved workspace could not be reopened; its files are left untouched.\n"
                      << "Reason: " << error.what() << '\n';
        }
    }

    const auto default_root = default_workspace_root(task);
    if (workspace_path_is_occupied(default_root)) {
        try {
            const Workspace workspace = open_workspace(default_root);
            if (workspace.task->id == task.id) {
                return workspace;
            }
        } catch (const std::exception&) {
            // An unrelated or invalid directory is never overwritten. A new suffix is used.
        }
    }
    return std::nullopt;
}

std::filesystem::path next_available_workspace_root(const Task& task) {
    const auto base = default_workspace_root(task);
    if (!workspace_path_is_occupied(base)) {
        return base;
    }

    for (unsigned suffix = 2U; suffix < 10000U; ++suffix) {
        auto candidate = base;
        candidate += "-" + std::to_string(suffix);
        if (!workspace_path_is_occupied(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Cannot find an unused workspace directory for task " + task.id + ".");
}

int guided_learn(std::string_view requested_task_id) {
    const Task* selected = nullptr;
    if (!requested_task_id.empty()) {
        selected = find_task(requested_task_id);
        if (selected == nullptr) {
            throw std::invalid_argument("Unknown task ID: " + std::string(requested_task_id));
        }

        std::string missing_prerequisite;
        if (!prerequisites_satisfied(*selected, &missing_prerequisite)) {
            throw std::invalid_argument("Task is locked: complete " + missing_prerequisite +
                                        " and pass its current acceptance check first.");
        }
    } else {
        for (const Task& task : task_catalog()) {
            if (has_current_pass(task.id)) {
                continue;
            }
            if (prerequisites_satisfied(task)) {
                selected = &task;
                break;
            }
        }
        if (selected == nullptr) {
            std::cout << "No unpassed task is currently available.\n"
                      << "Run 'cppapplab progress' to review the course, or choose a task with "
                         "'cppapplab learn <task-id>'.\n";
            return 0;
        }
    }

    std::optional<Workspace> existing = find_existing_workspace(*selected);
    const bool resumed = existing.has_value();
    Workspace workspace;
    if (existing.has_value()) {
        workspace = *existing;
    } else {
        workspace = create_workspace(*selected, next_available_workspace_root(*selected));
        register_workspace(workspace.root);
    }

    std::cout << "CppAppLab · Guided learning\n\n"
              << (resumed ? "Resuming your saved workspace\n" : "Created your learner workspace\n")
              << "Task: " << selected->id << " · " << selected->title << '\n'
              << "Workspace: " << workspace.root.string() << "\n\n"
              << "Files in this task:\n";
    for (const auto& source : selected->source_files) {
        std::cout << "  - " << source.generic_string() << '\n';
    }

    std::cout << "\n--- Task guide ---\n";
    const auto lesson = data_root() / selected->lesson_path;
    std::cout << read_lesson_file(lesson, kMaxLessonBytes);
    std::cout << "\n--- Your next steps ---\n"
              << "1. Open the workspace in your editor. VS Code users can run:\n"
              << "   code \"" << workspace.root.string() << "\"\n"
              << "2. If you need help, request the first hint:\n"
              << "   ./build/cppapplab hint \"" << workspace.root.string() << "\" 1\n"
              << "3. When your implementation is ready, run the fixed acceptance check:\n"
              << "   ./build/cppapplab check \"" << workspace.root.string() << "\"\n"
              << "4. After PASS, continue with the next available task:\n"
              << "   ./build/cppapplab learn\n\n"
              << "Your workspace and progress are saved between sessions. "
                 "Running 'learn' again resumes an incomplete task; after PASS it advances to the "
                 "next task.\n";
    return 0;
}

std::string task_state_label(const Task& task) {
    if (has_current_pass(task.id)) {
        return "PASSED";
    }

    const auto record = read_progress(task.id);
    if (record.has_value()) {
        if (!record->checked) {
            return "IN PROGRESS";
        }
        return record->evidence.passed ? "STALE" : "NEEDS WORK";
    }

    return prerequisites_satisfied(task) ? "AVAILABLE" : "LOCKED";
}

std::string normalize_task_query(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char character : value) {
        normalized.push_back(static_cast<char>(std::tolower(character)));
    }
    return normalized;
}

std::string trim_task_query(std::string_view value) {
    std::size_t first = 0U;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1U])) != 0) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

bool task_matches_query(const Task& task, std::string_view normalized_query) {
    const std::string normalized_id = normalize_task_query(task.id);
    const std::string normalized_title = normalize_task_query(task.title);
    return normalized_id.find(normalized_query) != std::string::npos ||
           normalized_title.find(normalized_query) != std::string::npos;
}

int open_selected_task(const Task& task) {
    try {
        return guided_learn(task.id);
    } catch (const std::exception& error) {
        std::cerr << "Cannot open this task: " << error.what() << '\n';
        return 1;
    }
}

int choose_task_interactively() {
    const auto& tasks = task_catalog();
    std::cout << "\nChoose a task by number, task ID, or title keyword (0 to return):\n";
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        const Task& task = tasks[index];
        std::cout << "  " << (index + 1U) << ". [" << task_state_label(task) << "] " << task.id
                  << " · " << task.title << '\n';
    }

    std::string selection;
    while (true) {
        std::cout << "Task: " << std::flush;
        if (!std::getline(std::cin, selection)) {
            return 0;
        }
        selection = trim_task_query(selection);
        if (selection.empty() || selection == "0") {
            return 0;
        }

        for (std::size_t index = 0; index < tasks.size(); ++index) {
            if (selection == std::to_string(index + 1U) || selection == tasks[index].id) {
                return open_selected_task(tasks[index]);
            }
        }

        const std::string normalized_query = normalize_task_query(selection);
        std::vector<std::size_t> matches;
        for (std::size_t index = 0; index < tasks.size(); ++index) {
            if (task_matches_query(tasks[index], normalized_query)) {
                matches.push_back(index);
            }
        }

        if (matches.size() == 1U) {
            return open_selected_task(tasks[matches.front()]);
        }
        if (matches.size() > 1U) {
            std::cout << "Several tasks match \"" << selection << "\":\n";
            for (const std::size_t index : matches) {
                std::cout << "  " << (index + 1U) << ". [" << task_state_label(tasks[index]) << "] "
                          << tasks[index].id << " · " << tasks[index].title << '\n';
            }
            std::cout << "Enter one of these numbers or a more specific keyword.\n";
        } else {
            std::cout << "No task matches \"" << selection
                      << "\". Try a number, task ID, or a word from its title.\n";
        }
    }
}

int interactive_menu() {
    const auto& tasks = task_catalog();
    while (true) {
        std::size_t passed = 0U;
        std::size_t in_progress = 0U;
        std::size_t needs_work = 0U;
        std::size_t stale = 0U;
        std::size_t available = 0U;
        std::size_t locked = 0U;

        for (const Task& task : tasks) {
            const std::string state = task_state_label(task);
            if (state == "PASSED") {
                ++passed;
            } else if (state == "IN PROGRESS") {
                ++in_progress;
            } else if (state == "NEEDS WORK") {
                ++needs_work;
            } else if (state == "STALE") {
                ++stale;
            } else if (state == "AVAILABLE") {
                ++available;
            } else {
                ++locked;
            }
        }

        std::cout << "\nCppAppLab · Learning home\n"
                  << "Course: " << passed << '/' << tasks.size() << " passed"
                  << " · in progress: " << in_progress << " · needs work: " << needs_work
                  << " · stale: " << stale << " · available: " << available
                  << " · locked: " << locked << "\n\n"
                  << "  1. Continue learning\n"
                  << "  2. Choose or search a task\n"
                  << "  3. View course progress\n"
                  << "  4. Help and commands\n"
                  << "  0. Exit\n"
                  << "Select [1]: " << std::flush;

        std::string choice;
        if (!std::getline(std::cin, choice)) {
            return 0;
        }

        try {
            if (choice.empty() || choice == "1") {
                (void)guided_learn({});
            } else if (choice == "2") {
                (void)choose_task_interactively();
            } else if (choice == "3") {
                (void)show_progress();
            } else if (choice == "4") {
                std::cout << usage();
            } else if (choice == "0" || choice == "q" || choice == "Q") {
                return 0;
            } else {
                std::cout << "Please enter 1, 2, 3, 4 or 0.\n";
            }
        } catch (const std::exception& error) {
            std::cerr << "CppAppLab: " << error.what() << '\n';
        }
    }
}

int show_history(std::string_view task_id) {
    const auto records = read_progress_history(task_id);
    if (records.empty()) {
        std::cout << "No check history exists for task " << task_id << ".\n";
        return 0;
    }

    std::cout << "Check history: " << task_id << '\n';
    for (const ProgressRecord& record : records) {
        std::cout << record.evidence.timestamp << "  " << (record.evidence.passed ? "PASS" : "FAIL")
                  << "  " << record.evidence.tests_passed << '/' << record.evidence.tests_total
                  << '\n'
                  << "  Workspace: " << record.workspace_root.string() << '\n'
                  << "  Fingerprint: " << record.evidence.fingerprint << '\n';
    }
    return 0;
}

void print_help() {
    std::cout << usage();
}

}  // namespace

int run_cli(int argc, char* argv[]) {
    if (argc <= 1) {
        if (::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0) {
            return interactive_menu();
        }
        print_help();
        return 0;
    }

    const std::string_view command{argv[1]};

    try {
        if (command == "--help" || command == "-h" || command == "help") {
            print_help();
            return 0;
        }
        if (command == "--version" || command == "-v") {
            std::cout << version() << '\n';
            return 0;
        }
        if (command == "menu") {
            if (argc != 2) {
                throw std::invalid_argument("Usage: cppapplab menu");
            }
            return interactive_menu();
        }
        if (command == "learn") {
            if (argc > 3) {
                throw std::invalid_argument("Usage: cppapplab learn [task-id]");
            }
            return guided_learn(argc == 3 ? std::string_view(argv[2]) : std::string_view{});
        }
        if (command == "list") {
            print_task_catalog();
            return 0;
        }
        if (command == "progress") {
            if (argc != 2) {
                throw std::invalid_argument("Usage: cppapplab progress");
            }
            return show_progress();
        }
        if (command == "resume") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab resume <task-id>");
            }
            if (find_task(argv[2]) == nullptr) {
                throw std::invalid_argument("Unknown task ID: " + std::string(argv[2]));
            }
            return resume_task(argv[2]);
        }
        if (command == "history") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab history <task-id>");
            }
            if (find_task(argv[2]) == nullptr) {
                throw std::invalid_argument("Unknown task ID: " + std::string(argv[2]));
            }
            return show_history(argv[2]);
        }
        if (command == "start") {
            if (argc != 4) {
                throw std::invalid_argument(
                    "Usage: cppapplab start <task-id> <workspace-directory>");
            }

            const Task* task = find_task(argv[2]);
            if (task == nullptr) {
                throw std::invalid_argument("Unknown task ID: " + std::string(argv[2]));
            }

            std::string missing_prerequisite;
            if (!prerequisites_satisfied(*task, &missing_prerequisite)) {
                throw std::invalid_argument("Task is locked: complete " + missing_prerequisite +
                                            " and pass its current acceptance check first.");
            }

            const Workspace workspace = create_workspace(*task, argv[3]);
            register_workspace(workspace.root);
            std::cout << "Created learner workspace: " << workspace.root.string() << '\n'
                      << "Task: " << task->id << '\n'
                      << "Next: cppapplab lesson \"" << workspace.root.string() << "\"\n";
            return 0;
        }
        if (command == "lesson") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab lesson <workspace-directory>");
            }

            const Workspace workspace = open_workspace(argv[2]);
            const auto lesson = data_root() / workspace.task->lesson_path;
            std::cout << read_lesson_file(lesson, kMaxLessonBytes);
            return 0;
        }
        if (command == "hint") {
            if (argc != 4) {
                throw std::invalid_argument("Usage: cppapplab hint <workspace-directory> <1|2|3>");
            }

            const Workspace workspace = open_workspace(argv[2]);
            const std::string_view level_text{argv[3]};
            if (level_text.size() != 1 || level_text[0] < '1' || level_text[0] > '3') {
                throw std::invalid_argument("Hint level must be 1, 2, or 3.");
            }

            const auto hints = data_root() / workspace.task->hints_path;
            const std::string content = read_lesson_file(hints, kMaxHintBytes);
            std::cout << extract_hint(content, static_cast<unsigned>(level_text[0] - '0'));
            return 0;
        }
        if (command == "check") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab check <workspace-directory>");
            }
            return check_workspace(argv[2]);
        }
        if (command == "status") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab status <workspace-directory>");
            }
            return show_status(argv[2]);
        }
        if (command == "next") {
            if (argc != 3) {
                throw std::invalid_argument("Usage: cppapplab next <workspace-directory>");
            }
            return show_next(argv[2]);
        }

        throw std::invalid_argument("Unknown command: " + std::string(command));
    } catch (const std::exception& error) {
        std::cerr << "CppAppLab error: " << error.what() << "\n"
                  << "Run 'cppapplab --help' for usage.\n";
        return 2;
    }
}

}  // namespace cppapplab
