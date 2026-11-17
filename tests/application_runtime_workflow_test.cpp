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
#include <utility>
#include <vector>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-application-runtime-test-" + std::to_string(stamp));
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
    ScopedEnvironment(std::string name, std::string value) : name_(std::move(name)) {
        if (const char* previous = std::getenv(name_.c_str())) {
            previous_ = previous;
            had_previous_ = true;
        }
        if (::setenv(name_.c_str(), value.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set test environment variable: " + name_);
        }
    }

    ~ScopedEnvironment() {
        if (had_previous_) {
            (void)::setenv(name_.c_str(), previous_.c_str(), 1);
        } else {
            (void)::unsetenv(name_.c_str());
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

   private:
    std::string name_;
    std::string previous_;
    bool had_previous_{false};
};

class StdoutCapture {
   public:
    StdoutCapture()
        : previous_(std::cout.rdbuf(stream_.rdbuf())),
          previous_error_(std::cerr.rdbuf(stream_.rdbuf())) {}
    ~StdoutCapture() {
        std::cout.rdbuf(previous_);
        std::cerr.rdbuf(previous_error_);
    }
    [[nodiscard]] std::string str() const { return stream_.str(); }

   private:
    std::ostringstream stream_;
    std::streambuf* previous_;
    std::streambuf* previous_error_;
};

int invoke(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
}

std::string invoke_capture(const std::vector<std::string>& arguments, int& result) {
    StdoutCapture capture;
    result = invoke(arguments);
    return capture.str();
}

void copy_reference(const std::filesystem::path& workspace,
                    const std::filesystem::path& reference,
                    const std::vector<std::string>& files) {
    for (const std::string& relative : files) {
        std::filesystem::copy_file(reference / relative, workspace / relative,
                                   std::filesystem::copy_options::overwrite_existing);
    }
}

}  // namespace

TEST(ApplicationRuntimeWorkflowTest, CalculatorBuildAndRunUsesFixedProfileWithoutWritingEvidence) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "calculator");
    const auto reference = cppapplab::data_root() / "labs/w1/calculator/reference";
    copy_reference(workspace.root, reference, {"src/calculator.cpp"});

    int result = -1;
    std::string output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("build is missing or stale"), std::string::npos);

    output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    EXPECT_NE(output.find("Application build result: PASS"), std::string::npos);

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("42"), std::string::npos);
    EXPECT_NE(output.find("Application result: PASS"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workspace.root / ".cppapplab" / "evidence.txt"));

    {
        std::ofstream changed(workspace.root / "src/calculator.cpp", std::ios::app);
        ASSERT_TRUE(changed.good());
        changed << "\n";
    }
    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "calculate", "add", "20", "22"},
        result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("stale"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, TodoApplicationStateIsIsolatedToItsWorkspace) {
    const cppapplab::Task* task = cppapplab::find_task("w2.todo-cli");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());
    const auto global_todo_file = temporary.path() / "global-todos.tsv";
    ScopedEnvironment global_todo("CPPAPPLAB_TODO_FILE", global_todo_file.string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "todo");
    const auto reference = cppapplab::data_root() / "labs/w2/todo/reference";
    copy_reference(workspace.root, reference, {"src/todo.cpp"});

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;

    output = invoke_capture(
        {"cppapplab", "app-run", workspace.root.string(), "add", "write browser tests"},
        result);
    EXPECT_EQ(result, 0) << output;
    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "list"}, result);
    EXPECT_EQ(result, 0) << output;
    EXPECT_NE(output.find("write browser tests"), std::string::npos);

    EXPECT_TRUE(std::filesystem::exists(
        workspace.root / ".cppapplab" / "application" / "runtime" / "todos.tsv"));
    EXPECT_FALSE(std::filesystem::exists(global_todo_file));
}

TEST(ApplicationRuntimeWorkflowTest, ThreadPoolDemoRunsThroughItsFixedApplicationProfile) {
    const cppapplab::Task* task = cppapplab::find_task("w6.thread-pool");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", temporary.path() / "state");

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "thread-pool");
    const auto reference = cppapplab::data_root() / "labs/w6/thread-pool/reference";
    copy_reference(workspace.root, reference, {"src/thread_pool.cpp"});

    int result = -1;
    std::string output = invoke_capture({"cppapplab", "app-build", workspace.root.string()}, result);
    ASSERT_EQ(result, 0) << output;
    output = invoke_capture({"cppapplab", "app-run", workspace.root.string(), "demo"}, result);
    // The learner's starter implementation is intentionally incomplete. The
    // browser must run that learner source and surface its TODO diagnostic,
    // rather than silently substituting the reference implementation.
    EXPECT_EQ(result, 1) << output;
    EXPECT_NE(output.find("TODO: implement ThreadPool::submit"), std::string::npos);
    EXPECT_NE(output.find("Application result: FAIL"), std::string::npos);
}

TEST(ApplicationRuntimeWorkflowTest, LibraryOnlyTaskHasNoApplicationExecutionProfile) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-connection-policy");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state("CPPAPPLAB_STATE_DIR", temporary.path() / "state");

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "connection-policy");
    int result = -1;
    const std::string output = invoke_capture(
        {"cppapplab", "app-build", workspace.root.string()}, result);
    EXPECT_EQ(result, 2);
    EXPECT_NE(output.find("no registered browser application profile"), std::string::npos);
}
