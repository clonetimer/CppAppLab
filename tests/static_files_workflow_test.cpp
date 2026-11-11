#include "cppapplab/learning.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-static-files-workflow-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

   private:
    std::filesystem::path path_;
};

class ScopedEnvironment {
   public:
    ScopedEnvironment(const char* name, const std::filesystem::path& value) : name_(name) {
        if (const char* previous = std::getenv(name_.c_str())) {
            previous_ = previous;
            had_previous_ = true;
        }
        const std::string text = value.string();
        if (::setenv(name_.c_str(), text.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set environment variable.");
        }
    }
    ~ScopedEnvironment() {
        if (had_previous_) {
            ::setenv(name_.c_str(), previous_.c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

   private:
    std::string name_;
    std::string previous_;
    bool had_previous_{false};
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create test file.");
    }
    output << content;
}

}  // namespace

TEST(StaticFilesWorkflowTest, PlatformCheckUsesFixedFixtureAndReferenceImplementation) {
    const cppapplab::Task* task = cppapplab::find_task("w8.static-files");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "static-files");
    const auto reference = cppapplab::data_root() / "labs/w8/static-files/reference";
    const std::vector<std::filesystem::path> reference_files{
        "include/http_message.hpp",
        "include/static_file_handler.hpp",
        "src/http_message.cpp",
        "src/static_file_handler.cpp",
        "src/main.cpp",
        "tests/static_file_handler_test.cpp",
    };
    for (const auto& relative_path : reference_files) {
        std::filesystem::copy_file(reference / relative_path, workspace.root / relative_path,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    write_file(workspace.root / "CMakeLists.txt",
               "message(FATAL_ERROR \"Learner CMakeLists must not run\")\n");

    std::vector<std::string> arguments{"cppapplab", "check", workspace.root.string()};
    std::vector<char*> argv;
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    const int result = cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());

    std::string diagnostic;
    if (result != 0) {
        std::ifstream log(workspace.root / ".cppapplab/last-check.log", std::ios::binary);
        if (log) {
            diagnostic.assign(std::istreambuf_iterator<char>(log),
                              std::istreambuf_iterator<char>());
        }
    }

    EXPECT_EQ(result, 0) << diagnostic;
    const auto evidence = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(evidence.has_value()) << diagnostic;
    EXPECT_TRUE(evidence->passed) << diagnostic;
    EXPECT_EQ(evidence->tests_passed, 31U) << diagnostic;
    EXPECT_EQ(evidence->tests_total, 31U) << diagnostic;
}

TEST(StaticFilesWorkflowTest, W9SecureStaticRootUsesLearnerSourcesInFixedAcceptance) {
    const cppapplab::Task* task = cppapplab::find_task("w9.static-root");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace =
        cppapplab::create_workspace(*task, temporary.path() / "secure-static-root");
    const auto reference = cppapplab::data_root() / "labs/w9/static-root/reference";
    for (const auto& relative_path : task->source_files) {
        std::filesystem::copy_file(reference / relative_path, workspace.root / relative_path,
                                   std::filesystem::copy_options::overwrite_existing);
    }

    write_file(workspace.root / "CMakeLists.txt",
               "message(FATAL_ERROR \"Learner CMakeLists must not run\")\n");

    std::vector<std::string> arguments{"cppapplab", "check", workspace.root.string()};
    std::vector<char*> argv;
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    const int result = cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());

    std::string diagnostic;
    if (result != 0) {
        std::ifstream log(workspace.root / ".cppapplab/last-check.log", std::ios::binary);
        if (log) {
            diagnostic.assign(std::istreambuf_iterator<char>(log),
                              std::istreambuf_iterator<char>());
        }
    }

    EXPECT_EQ(result, 0) << diagnostic;
    const auto evidence = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(evidence.has_value()) << diagnostic;
    EXPECT_TRUE(evidence->passed) << diagnostic;
    EXPECT_EQ(evidence->tests_passed, 34U) << diagnostic;
    EXPECT_EQ(evidence->tests_total, 34U) << diagnostic;
}
