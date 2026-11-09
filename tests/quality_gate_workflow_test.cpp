#include "cppapplab/learning.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-quality-gate-workflow-test-" + std::to_string(stamp));
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
        const std::string value_string = value.string();
        if (::setenv(name_.c_str(), value_string.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set test environment variable.");
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

class StdoutCapture {
   public:
    StdoutCapture() : previous_(std::cout.rdbuf(stream_.rdbuf())) {}

    ~StdoutCapture() {
        std::cout.rdbuf(previous_);
    }

    [[nodiscard]] std::string str() const {
        return stream_.str();
    }

   private:
    std::ostringstream stream_;
    std::streambuf* previous_;
};

struct CheckOutcome {
    int exit_code{};
    std::string output;
};

int invoke(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open test file: " + path.string());
    }
    output << content;
    output.flush();
    if (!output) {
        throw std::runtime_error("Cannot write test file: " + path.string());
    }
}

void copy_reference_sources(const std::filesystem::path& workspace_root) {
    const auto reference_root = cppapplab::data_root() / "labs/w4/quality-gate/reference/src";
    for (const char* source : {"task.cpp", "task_manager.cpp", "main.cpp"}) {
        std::filesystem::copy_file(reference_root / source, workspace_root / "src" / source,
                                   std::filesystem::copy_options::overwrite_existing);
    }
    std::filesystem::copy_file(
        reference_root / "task_manager_test.cpp",
        workspace_root / "tests/task_manager_test.cpp",
        std::filesystem::copy_options::overwrite_existing
    );
}

CheckOutcome run_platform_check(const std::filesystem::path& workspace_root) {
    CheckOutcome outcome;
    {
        StdoutCapture capture;
        outcome.exit_code = invoke({"cppapplab", "check", workspace_root.string()});
        outcome.output = capture.str();
    }

    if (outcome.exit_code != 0) {
        std::ifstream log_file(workspace_root / ".cppapplab/last-check.log", std::ios::binary);
        if (log_file) {
            outcome.output += "\n--- acceptance log ---\n";
            outcome.output.append(std::istreambuf_iterator<char>(log_file),
                                  std::istreambuf_iterator<char>());
        }
    }
    return outcome;
}

}  // namespace

TEST(QualityGateWorkflowTest, FixedFixtureChecksRefactorAndPreservesCliPersistence) {
    const cppapplab::Task* task = cppapplab::find_task("w4.quality-gate");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "task-manager");

    copy_reference_sources(workspace.root);

    // The platform must use its fixed fixture, not learner-controlled CMake files.
    write_file(workspace.root / "CMakeLists.txt",
               "message(FATAL_ERROR \"Learner CMakeLists must not be executed by platform "
               "acceptance\")\n");

    const CheckOutcome outcome = run_platform_check(workspace.root);

    EXPECT_EQ(outcome.exit_code, 0) << outcome.output;
    EXPECT_NE(outcome.output.find("Result: PASS"), std::string::npos);
    EXPECT_NE(outcome.output.find("Acceptance: 41/41"), std::string::npos);

    const auto evidence = cppapplab::read_evidence(workspace.root);
    if (!evidence.has_value()) {
        FAIL() << "Platform check did not write acceptance evidence.";
        return;
    }

    const auto& record = evidence.value();
    EXPECT_TRUE(record.passed);
    EXPECT_EQ(record.tests_passed, 41U);
    EXPECT_EQ(record.tests_total, 41U);
}
