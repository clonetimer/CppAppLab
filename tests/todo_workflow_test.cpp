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
                ("cppapplab-todo-workflow-test-" + std::to_string(stamp));
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

}  // namespace

TEST(TodoWorkflowTest, FixedFixtureChecksLearnerImplementationAndCliPersistence) {
    const cppapplab::Task* task = cppapplab::find_task("w2.todo-cli");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", (temporary.path() / "state").string());

    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "todo");
    const auto reference_source = cppapplab::data_root() / "labs/w2/todo/reference/src/todo.cpp";
    std::filesystem::copy_file(reference_source, workspace.root / "src/todo.cpp",
                               std::filesystem::copy_options::overwrite_existing);

    // The platform must ignore learner-controlled build configuration.
    write_file(workspace.root / "CMakeLists.txt",
               "message(FATAL_ERROR \"Learner CMakeLists must not be executed by platform "
               "acceptance\")\n");

    int check_result = -1;
    std::string check_output;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", workspace.root.string()});
        check_output = capture.str();
    }

    if (check_result != 0) {
        std::ifstream log_file(workspace.root / ".cppapplab/last-check.log", std::ios::binary);
        if (log_file) {
            check_output += "\n--- acceptance log ---\n";
            check_output.append(std::istreambuf_iterator<char>(log_file),
                                std::istreambuf_iterator<char>());
        }
    }

    EXPECT_EQ(check_result, 0) << check_output;
    EXPECT_NE(check_output.find("Result: PASS"), std::string::npos);
    EXPECT_NE(check_output.find("Acceptance: 19/19"), std::string::npos);

    const auto evidence = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(evidence.has_value());
    EXPECT_TRUE(evidence->passed);
    EXPECT_EQ(evidence->tests_passed, 19U);
    EXPECT_EQ(evidence->tests_total, 19U);
}
