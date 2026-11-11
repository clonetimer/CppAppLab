#include "cppapplab/learning.hpp"
#include "cppapplab/progress.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <fstream>
#include <iostream>
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
                ("cppapplab-learning-test-" + std::to_string(stamp));
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

class EnvironmentOverride {
   public:
    EnvironmentOverride(std::string name, std::string value) : name_(std::move(name)) {
        const char* previous = std::getenv(name_.c_str());
        if (previous != nullptr) {
            had_previous_ = true;
            previous_ = previous;
        }
        if (::setenv(name_.c_str(), value.c_str(), 1) != 0) {
            throw std::runtime_error("Cannot set test environment variable: " + name_);
        }
    }

    ~EnvironmentOverride() {
        if (had_previous_) {
            ::setenv(name_.c_str(), previous_.c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

    EnvironmentOverride(const EnvironmentOverride&) = delete;
    EnvironmentOverride& operator=(const EnvironmentOverride&) = delete;

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

void write_source(const std::filesystem::path& workspace, const std::string& source) {
    std::ofstream output(workspace / "src/main.cpp", std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open learner source for the integration test.");
    }
    output << source;
    output.flush();
    if (!output) {
        throw std::runtime_error("Cannot write learner source for the integration test.");
    }
}

}  // namespace

TEST(GuidedLearningWorkflowTest, LearnCreatesAndThenResumesTheDefaultWorkspace) {
    TemporaryDirectory temporary;
    const auto home = temporary.path() / "home";
    EnvironmentOverride home_override("HOME", home.string());
    EnvironmentOverride state_override("CPPAPPLAB_STATE_DIR",
                                       (temporary.path() / "state").string());

    std::string first_output;
    {
        StdoutCapture capture;
        EXPECT_EQ(invoke({"cppapplab", "learn"}), 0);
        first_output = capture.str();
    }

    const auto expected_workspace = home / "cppapplab-work" / "w1.hello-world";
    EXPECT_NE(first_output.find("Created your learner workspace"), std::string::npos);
    EXPECT_NE(first_output.find("First C++ Program"), std::string::npos);
    EXPECT_NE(first_output.find("cppapplab check"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(expected_workspace / ".cppapplab-workspace"));

    const auto saved = cppapplab::read_progress("w1.hello-world");
    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->workspace_root, std::filesystem::absolute(expected_workspace));

    std::string second_output;
    {
        StdoutCapture capture;
        EXPECT_EQ(invoke({"cppapplab", "learn", "w1.hello-world"}), 0);
        second_output = capture.str();
    }
    EXPECT_NE(second_output.find("Resuming your saved workspace"), std::string::npos);
    EXPECT_NE(second_output.find(expected_workspace.string()), std::string::npos);
}

TEST(GuidedLearningWorkflowTest, LearnNeverOverwritesAnUnrelatedDefaultDirectory) {
    TemporaryDirectory temporary;
    const auto home = temporary.path() / "home";
    const auto occupied = home / "cppapplab-work" / "w1.hello-world";
    std::filesystem::create_directories(occupied);
    {
        std::ofstream sentinel(occupied / "keep.txt");
        sentinel << "keep this file";
    }
    EnvironmentOverride home_override("HOME", home.string());
    EnvironmentOverride state_override("CPPAPPLAB_STATE_DIR",
                                       (temporary.path() / "state").string());

    std::string output;
    {
        StdoutCapture capture;
        EXPECT_EQ(invoke({"cppapplab", "learn"}), 0);
        output = capture.str();
    }
    const auto new_workspace = home / "cppapplab-work" / "w1.hello-world-2";
    EXPECT_NE(output.find(new_workspace.string()), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(new_workspace / ".cppapplab-workspace"));
    std::ifstream sentinel(occupied / "keep.txt");
    std::string content;
    std::getline(sentinel, content);
    EXPECT_EQ(content, "keep this file");
}

TEST(GuidedLearningWorkflowTest, LearnMovesToTheNextUnlockedTaskAfterAcceptancePasses) {
    TemporaryDirectory temporary;
    const auto home = temporary.path() / "home";
    EnvironmentOverride home_override("HOME", home.string());
    EnvironmentOverride state_override("CPPAPPLAB_STATE_DIR",
                                       (temporary.path() / "state").string());

    ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
    const auto first_workspace = home / "cppapplab-work" / "w1.hello-world";
    write_source(first_workspace,
                 "#include <iostream>\n"
                 "int main() {\n"
                 "    std::cout << \"Hello, CppAppLab!\\n\";\n"
                 "    return 0;\n"
                 "}\n");

    std::string check_output;
    int check_result = -1;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", first_workspace.string()});
        check_output = capture.str();
    }
    ASSERT_EQ(check_result, 0) << check_output;

    std::string next_output;
    {
        StdoutCapture capture;
        EXPECT_EQ(invoke({"cppapplab", "learn"}), 0);
        next_output = capture.str();
    }
    EXPECT_NE(next_output.find("w1.calculator"), std::string::npos);
    EXPECT_NE(next_output.find("Created your learner workspace"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_regular_file(home / "cppapplab-work" / "w1.calculator" /
                                                 ".cppapplab-workspace"));
}

TEST(LearningWorkflowTest, FixedCheckPassesAndSourceEditsMakeEvidenceStale) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "learner");

    write_source(workspace.root,
                 "#include <iostream>\n"
                 "int main() {\n"
                 "    std::cout << \"Hello, CppAppLab!\\n\";\n"
                 "    return 0;\n"
                 "}\n");

    int check_result = -1;
    std::string check_output;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", workspace.root.string()});
        check_output = capture.str();
    }

    EXPECT_EQ(check_result, 0) << check_output;
    EXPECT_NE(check_output.find("Result: PASS"), std::string::npos);

    const auto evidence = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(evidence.has_value());
    EXPECT_TRUE(evidence->passed);
    EXPECT_EQ(evidence->tests_passed, 1U);
    EXPECT_EQ(evidence->tests_total, 1U);

    {
        StdoutCapture capture;
        const int next_result = invoke({"cppapplab", "next", workspace.root.string()});
        EXPECT_EQ(next_result, 0);
        EXPECT_NE(capture.str().find("w1.calculator"), std::string::npos);
    }

    write_source(workspace.root,
                 "#include <iostream>\n"
                 "int main() {\n"
                 "    std::cout << \"not the required output\\n\";\n"
                 "    return 0;\n"
                 "}\n");

    int status_result = -1;
    std::string status_output;
    {
        StdoutCapture capture;
        status_result = invoke({"cppapplab", "status", workspace.root.string()});
        status_output = capture.str();
    }

    EXPECT_EQ(status_result, 0);
    EXPECT_NE(status_output.find("STALE"), std::string::npos);
}

TEST(LearningWorkflowTest, LessonAndProgressiveHintsAreReadOnlyCommands) {
    const cppapplab::Task* task = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "learner");
    const std::string before = cppapplab::source_fingerprint(*task, workspace.root);

    for (const std::string level : {"1", "2", "3"}) {
        StdoutCapture capture;
        const int result = invoke({"cppapplab", "hint", workspace.root.string(), level});
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(capture.str().empty());
    }

    {
        StdoutCapture capture;
        const int result = invoke({"cppapplab", "lesson", workspace.root.string()});
        EXPECT_EQ(result, 0);
        EXPECT_NE(capture.str().find("First C++ Program"), std::string::npos);
    }

    const std::string after = cppapplab::source_fingerprint(*task, workspace.root);
    EXPECT_EQ(before, after);
}
