#include "cppapplab/learning.hpp"

#include "cppapplab/app.hpp"
#include "cppapplab/process.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppapplab {
namespace {

constexpr std::size_t kMaxLessonBytes = 64U * 1024U;
constexpr std::size_t kMaxHintBytes = 32U * 1024U;
constexpr std::size_t kMaxCompilerOutput = 128U * 1024U;
constexpr std::size_t kMaxProgramOutput = 16U * 1024U;

std::string read_lesson_file(
    const std::filesystem::path& path,
    std::size_t max_bytes
) {
    const auto status = std::filesystem::symlink_status(path);
    if (!std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
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

    std::string content{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
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
    const std::size_t body_end =
        next_heading == std::string::npos ? content.size() : next_heading;
    return content.substr(body_start + 1, body_end - body_start - 1);
}

void print_task_catalog() {
    std::cout << "CppAppLab · Available tasks\n\n";
    for (const Task& task : task_catalog()) {
        std::cout << task.id << "  |  " << task.title << "  |  W" << task.week << '\n';
    }
    std::cout << "\nStart a task with: cppapplab start <task-id> <workspace-directory>\n";
}

void ensure_real_directory(const std::filesystem::path& path) {
    if (std::filesystem::exists(path) || std::filesystem::is_symlink(path)) {
        const auto status = std::filesystem::symlink_status(path);
        if (!std::filesystem::is_directory(status) ||
            std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Refusing to use a non-directory or symlink: " +
                                     path.string());
        }
    } else {
        std::filesystem::create_directory(path);
    }
}

int check_workspace(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    const Task& task = *workspace.task;

    if (task.acceptance_kind != AcceptanceKind::ExactStdout ||
        task.source_files.size() != 1) {
        throw std::runtime_error("This task's acceptance runner is not implemented yet.");
    }

    const std::string fingerprint_before =
        source_fingerprint(task, workspace.root);

    const auto state_directory = workspace.root / ".cppapplab";
    ensure_real_directory(state_directory);

    const auto build_directory = state_directory / "build";
    ensure_real_directory(build_directory);

    const auto executable = build_directory / "learner_app";
    if (std::filesystem::exists(executable) || std::filesystem::is_symlink(executable)) {
        const auto status = std::filesystem::symlink_status(executable);
        if (!std::filesystem::is_regular_file(status) ||
            std::filesystem::is_symlink(status)) {
            throw std::runtime_error("Refusing to replace a non-regular build output.");
        }
        std::filesystem::remove(executable);
    }

    const auto source = workspace.root / task.source_files.front();
    const std::vector<std::string> compile_arguments{
        "g++",
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-g",
        source.string(),
        "-o",
        executable.string()
    };

    std::ostringstream log;
    log << "$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -g "
        << task.source_files.front().generic_string()
        << " -o .cppapplab/build/learner_app\n";

    const ProcessResult compilation = run_process(
        compile_arguments,
        workspace.root,
        std::chrono::seconds(30),
        kMaxCompilerOutput
    );
    log << compilation.output;

    bool passed = false;
    if (compilation.timed_out) {
        log << "\n[CppAppLab] Compilation exceeded the 30-second time limit.\n";
    } else if (compilation.output_truncated) {
        log << "\n[CppAppLab] Compiler output exceeded the capture limit.\n";
    } else if (compilation.exit_code != 0) {
        log << "\n[CppAppLab] Compilation failed with exit code "
            << compilation.exit_code << ".\n";
    } else {
        log << "\n$ .cppapplab/build/learner_app\n";
        const ProcessResult execution = run_process(
            {executable.string()},
            workspace.root,
            std::chrono::seconds(5),
            kMaxProgramOutput
        );
        log << execution.output;

        if (execution.timed_out) {
            log << "\n[CppAppLab] Program exceeded the 5-second time limit.\n";
        } else if (execution.output_truncated) {
            log << "\n[CppAppLab] Program output exceeded the capture limit.\n";
        } else if (execution.exit_code != 0) {
            log << "\n[CppAppLab] Program exited with code "
                << execution.exit_code << ".\n";
        } else if (execution.output != task.expected_stdout) {
            log << "\n[CppAppLab] Output did not match the task contract.\n"
                << "Expected bytes: " << task.expected_stdout.size() << '\n'
                << "Actual bytes: " << execution.output.size() << '\n';
        } else {
            passed = true;
        }
    }

    const std::string fingerprint_after =
        source_fingerprint(task, workspace.root);
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

    std::cout << "Task: " << task.id << '\n'
              << "Result: " << (passed ? "PASS" : "FAIL") << '\n'
              << "Acceptance: " << evidence.tests_passed << '/'
              << evidence.tests_total << '\n'
              << "Source fingerprint: " << evidence.fingerprint << '\n'
              << "Evidence: " << (workspace.root / ".cppapplab/evidence.txt").string()
              << '\n'
              << "Log: " << (workspace.root / ".cppapplab/last-check.log").string()
              << '\n';

    return passed ? 0 : 1;
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

    const std::string current_fingerprint =
        source_fingerprint(*workspace.task, workspace.root);

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
              << "Acceptance: " << evidence->tests_passed << '/'
              << evidence->tests_total << '\n'
              << "Checked at: " << evidence->timestamp << '\n'
              << "Source fingerprint: " << evidence->fingerprint << '\n'
              << "Note: local evidence is a navigation record, not a certificate; "
                 "rerun check to verify the current source.\n";
    return 0;
}

int show_next(const std::filesystem::path& workspace_path) {
    const Workspace workspace = open_workspace(workspace_path);
    const auto evidence = read_evidence(workspace.root);

    if (!evidence.has_value() || !evidence->passed ||
        evidence->fingerprint != source_fingerprint(*workspace.task, workspace.root)) {
        std::cout << "The current task has no recorded PASS for its current source.\n"
                  << "Complete the implementation and run cppapplab check first.\n";
        return 1;
    }

    std::cout << "Current task has a recorded PASS: " << workspace.task->id << "\n"
              << "Next planned work product: W1 Calculator (not yet available as a learner task).\n"
              << "This is learning navigation only; local evidence is not a completion certificate.\n";
    return 0;
}

void print_help() {
    std::cout << usage();
}

}  // namespace

int run_cli(int argc, char* argv[]) {
    if (argc <= 1) {
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
        if (command == "list") {
            print_task_catalog();
            return 0;
        }
        if (command == "start") {
            if (argc != 4) {
                throw std::invalid_argument(
                    "Usage: cppapplab start <task-id> <workspace-directory>"
                );
            }

            const Task* task = find_task(argv[2]);
            if (task == nullptr) {
                throw std::invalid_argument("Unknown task ID: " + std::string(argv[2]));
            }

            const Workspace workspace = create_workspace(*task, argv[3]);
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
