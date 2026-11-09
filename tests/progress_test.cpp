#include "cppapplab/progress.hpp"
#include "cppapplab/learning.hpp"

#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <optional>
#include <string>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-progress-test-" + std::to_string(stamp));
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

class ScopedStateDirectory {
public:
    explicit ScopedStateDirectory(const std::filesystem::path& path) {
        if (const char* previous = std::getenv("CPPAPPLAB_STATE_DIR")) {
            previous_ = previous;
            had_previous_ = true;
        }
        if (::setenv("CPPAPPLAB_STATE_DIR", path.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set CPPAPPLAB_STATE_DIR for test.");
        }
    }

    ~ScopedStateDirectory() {
        if (had_previous_) {
            ::setenv("CPPAPPLAB_STATE_DIR", previous_.c_str(), 1);
        } else {
            ::unsetenv("CPPAPPLAB_STATE_DIR");
        }
    }

private:
    std::string previous_;
    bool had_previous_{false};
};

void write_hello_source(
    const std::filesystem::path& workspace,
    const std::string& suffix = ""
) {
    std::ofstream output(
        workspace / "src/main.cpp",
        std::ios::binary | std::ios::trunc
    );
    if (!output) {
        throw std::runtime_error("Cannot open learner source.");
    }
    output << "#include <iostream>\n"
           << "int main() {\n"
           << "    std::cout << \"Hello, CppAppLab!\\n\";\n"
           << "    return 0;\n"
           << "}\n"
           << suffix;
    output.flush();
    if (!output) {
        throw std::runtime_error("Cannot write learner source.");
    }
}

cppapplab::Workspace create_hello_workspace(
    const std::filesystem::path& parent
) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    if (task == nullptr) {
        throw std::runtime_error("Hello World task is missing from the catalog.");
    }
    return cppapplab::create_workspace(*task, parent / "hello");
}

cppapplab::Evidence save_result(
    const cppapplab::Workspace& workspace,
    bool passed
) {
    cppapplab::Evidence evidence;
    evidence.task_id = workspace.task->id;
    evidence.fingerprint =
        cppapplab::source_fingerprint(*workspace.task, workspace.root);
    evidence.passed = passed;
    evidence.tests_passed = passed ? 1U : 0U;
    evidence.tests_total = 1U;
    evidence.timestamp = passed ? "2026-11-09T06:00:00Z" : "2026-11-09T06:01:00Z";

    cppapplab::write_evidence(workspace.root, evidence, "progress test log\n");
    cppapplab::write_progress(workspace.root, evidence);
    return evidence;
}

}  // namespace

TEST(ProgressTest, StartingWorkspaceRegistersRecoverableInProgressState) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");

    const std::vector<std::string> arguments{
        "cppapplab",
        "start",
        "w1.hello-world",
        (temporary.path() / "hello-started").string()
    };
    std::vector<char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    const int result = cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    std::cout.rdbuf(previous);

    EXPECT_EQ(result, 0);
    const auto saved = cppapplab::read_progress("w1.hello-world");
    ASSERT_TRUE(saved.has_value());
    EXPECT_FALSE(saved->checked);
    EXPECT_FALSE(saved->evidence.passed);
    EXPECT_TRUE(std::filesystem::is_directory(saved->workspace_root));
    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));
}

TEST(ProgressTest, StartBlocksCalculatorBeforeItsPrerequisitePasses) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");

    const std::vector<std::string> arguments{
        "cppapplab",
        "start",
        "w1.calculator",
        (temporary.path() / "calculator").string()
    };
    std::vector<char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    const int result = cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    EXPECT_EQ(result, 2);
    EXPECT_FALSE(std::filesystem::exists(temporary.path() / "calculator"));
}

TEST(ProgressTest, ResumeFindsWorkspaceBeforeFirstCheck) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const auto workspace = create_hello_workspace(temporary.path());
    cppapplab::register_workspace(workspace.root);

    const std::vector<std::string> arguments{
        "cppapplab",
        "resume",
        "w1.hello-world"
    };
    std::vector<char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    const int result = cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    std::cout.rdbuf(previous);

    EXPECT_EQ(result, 0);
    EXPECT_NE(output.str().find(workspace.root.string()), std::string::npos);
    EXPECT_NE(output.str().find("NOT CHECKED"), std::string::npos);
}

TEST(ProgressTest, PersistsCurrentPassAndWorkspaceForResume) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const auto workspace = create_hello_workspace(temporary.path());

    write_hello_source(workspace.root);
    const auto evidence = save_result(workspace, true);

    const auto saved = cppapplab::read_progress("w1.hello-world");
    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->workspace_root, workspace.root);
    EXPECT_EQ(saved->evidence.fingerprint, evidence.fingerprint);
    EXPECT_TRUE(saved->evidence.passed);
    EXPECT_TRUE(cppapplab::has_current_pass("w1.hello-world"));

    const auto history = cppapplab::read_progress_history("w1.hello-world");
    ASSERT_EQ(history.size(), 1U);
    EXPECT_TRUE(history.front().evidence.passed);
}

TEST(ProgressTest, SourceChangesInvalidatePreviouslySavedPass) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const auto workspace = create_hello_workspace(temporary.path());

    write_hello_source(workspace.root);
    (void)save_result(workspace, true);
    EXPECT_TRUE(cppapplab::has_current_pass("w1.hello-world"));

    write_hello_source(workspace.root, "// learner changed the source\n");
    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));
}

TEST(ProgressTest, CalculatorRemainsLockedUntilHelloWorldHasCurrentPass) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const cppapplab::Task* calculator = cppapplab::find_task("w1.calculator");
    ASSERT_NE(calculator, nullptr);

    std::string missing;
    EXPECT_FALSE(cppapplab::prerequisites_satisfied(*calculator, &missing));
    EXPECT_EQ(missing, "w1.hello-world");

    const auto workspace = create_hello_workspace(temporary.path());
    write_hello_source(workspace.root);
    (void)save_result(workspace, true);

    EXPECT_TRUE(cppapplab::prerequisites_satisfied(*calculator));
}

TEST(ProgressTest, FailedAttemptDoesNotUnlockDependentTask) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const cppapplab::Task* calculator = cppapplab::find_task("w1.calculator");
    ASSERT_NE(calculator, nullptr);

    const auto workspace = create_hello_workspace(temporary.path());
    write_hello_source(workspace.root);
    (void)save_result(workspace, false);

    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));
    EXPECT_FALSE(cppapplab::prerequisites_satisfied(*calculator));
}

TEST(ProgressTest, KeepsMultipleAttemptsAndUsesLatestResult) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const auto workspace = create_hello_workspace(temporary.path());

    write_hello_source(workspace.root);
    (void)save_result(workspace, true);
    write_hello_source(workspace.root, "// edited after first attempt\n");
    (void)save_result(workspace, false);

    const auto latest = cppapplab::read_progress("w1.hello-world");
    ASSERT_TRUE(latest.has_value());
    EXPECT_FALSE(latest->evidence.passed);
    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));

    const auto history = cppapplab::read_progress_history("w1.hello-world");
    ASSERT_EQ(history.size(), 2U);
    EXPECT_TRUE(history[0].evidence.passed);
    EXPECT_FALSE(history[1].evidence.passed);
}

TEST(ProgressTest, MissingWorkspaceInvalidatesSavedPass) {
    TemporaryDirectory temporary;
    ScopedStateDirectory state(temporary.path() / "state");
    const auto workspace = create_hello_workspace(temporary.path());

    write_hello_source(workspace.root);
    (void)save_result(workspace, true);
    EXPECT_TRUE(cppapplab::has_current_pass("w1.hello-world"));

    std::filesystem::remove_all(workspace.root);
    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));
}
