#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "cppapplab/learning.hpp"
#include "cppapplab/progress.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"
#include "windows_state_io.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

class Scratch {
   public:
    Scratch() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = fs::temp_directory_path() /
                (L"CppAppLab native learning " + std::to_wstring(stamp));
        if (!fs::create_directory(root_)) {
            throw std::runtime_error("Cannot create native learning scratch directory.");
        }
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    [[nodiscard]] const fs::path& root() const noexcept { return root_; }

   private:
    fs::path root_;
};

class ScopedPathEnvironment {
   public:
    ScopedPathEnvironment(const wchar_t* name, const fs::path& value)
        : name_(name), previous_(cppapplab::windows_state_io::environment_path(name)) {
        if (!SetEnvironmentVariableW(name_.c_str(), value.c_str())) {
            throw std::runtime_error("Cannot set isolated Windows path environment.");
        }
    }
    ~ScopedPathEnvironment() {
        (void)SetEnvironmentVariableW(name_.c_str(),
                                      previous_ ? previous_->c_str() : nullptr);
    }
    ScopedPathEnvironment(const ScopedPathEnvironment&) = delete;
    ScopedPathEnvironment& operator=(const ScopedPathEnvironment&) = delete;

   private:
    std::wstring name_;
    std::optional<fs::path> previous_;
};

class StdoutCapture {
   public:
    StdoutCapture() : previous_(std::cout.rdbuf(stream_.rdbuf())) {}
    ~StdoutCapture() { std::cout.rdbuf(previous_); }
    [[nodiscard]] std::string str() const { return stream_.str(); }

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

void write_source(const fs::path& workspace, const std::string& source) {
    std::ofstream output(workspace / "src" / "main.cpp", std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open learner source in native workspace.");
    }
    output.write(source.data(), static_cast<std::streamsize>(source.size()));
    output.flush();
    if (!output) {
        throw std::runtime_error("Cannot write learner source in native workspace.");
    }
}

struct IsolatedProfile {
    explicit IsolatedProfile(const fs::path& root)
        : home(root / L"用户 配置"),
          state(root / L"课程进度 状态"),
          user_profile(L"USERPROFILE", home),
          state_directory(L"CPPAPPLAB_STATE_DIR", state) {
        fs::create_directories(home);
    }

    fs::path home;
    fs::path state;
    ScopedPathEnvironment user_profile;
    ScopedPathEnvironment state_directory;
};

TEST(NativeLearningWorkflow, LearnCreatesAndReopensWorkspaceUnderUnicodeUserProfile) {
    Scratch scratch;
    IsolatedProfile profile(scratch.root());

    std::string first_output;
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
        first_output = capture.str();
    }

    const auto workspace = profile.home / "cppapplab-work" / "w1.hello-world";
    EXPECT_NE(first_output.find("Created your learner workspace"), std::string::npos);
    EXPECT_NE(first_output.find("First C++ Program"), std::string::npos);
    EXPECT_TRUE(fs::is_regular_file(workspace / ".cppapplab-workspace"));

    const auto saved = cppapplab::read_progress("w1.hello-world");
    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->workspace_root, fs::absolute(workspace));

    std::string resumed_output;
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn", "w1.hello-world"}), 0);
        resumed_output = capture.str();
    }
    EXPECT_NE(resumed_output.find("Resuming your saved workspace"), std::string::npos);
    EXPECT_NE(resumed_output.find(workspace.u8string()), std::string::npos);
}

TEST(NativeLearningWorkflow, RealW1CompileCheckPersistsEvidenceAndUnlocksCalculator) {
    Scratch scratch;
    IsolatedProfile profile(scratch.root());

    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
    }
    const auto workspace = profile.home / "cppapplab-work" / "w1.hello-world";
    write_source(workspace,
                 "#include <iostream>\n"
                 "int main() {\n"
                 "    std::cout << \"Hello, CppAppLab!\\n\";\n"
                 "    return 0;\n"
                 "}\n");

    std::string check_output;
    int check_result = -1;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", workspace.u8string()});
        check_output = capture.str();
    }
    const std::string full_log = cppapplab::windows_state_io::read(
        workspace / ".cppapplab" / "last-check.log", 128U * 1024U);
    ASSERT_EQ(check_result, 0) << check_output << "\nFull compiler log:\n" << full_log;
    EXPECT_NE(check_output.find("Result: PASS"), std::string::npos);
    EXPECT_NE(check_output.find("Acceptance: 1/1"), std::string::npos);
    EXPECT_TRUE(cppapplab::has_current_pass("w1.hello-world"));

    const auto evidence = cppapplab::read_evidence(workspace);
    ASSERT_TRUE(evidence.has_value());
    EXPECT_TRUE(evidence->passed);
    EXPECT_EQ(evidence->tests_passed, 1U);
    EXPECT_EQ(evidence->tests_total, 1U);

    const auto history = cppapplab::read_progress_history("w1.hello-world");
    ASSERT_EQ(history.size(), 1U);
    EXPECT_TRUE(history.back().evidence.passed);

    std::string next_output;
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
        next_output = capture.str();
    }
    EXPECT_NE(next_output.find("w1.calculator"), std::string::npos);
    EXPECT_TRUE(fs::is_regular_file(profile.home / "cppapplab-work" / "w1.calculator" /
                                    ".cppapplab-workspace"));
}

TEST(NativeLearningWorkflow, RealW1ThroughW4CMakeGoogleTestAcceptanceUsesSharedEngine) {
    Scratch scratch;
    IsolatedProfile profile(scratch.root());

    const auto* hello = cppapplab::find_task("w1.hello-world");
    ASSERT_NE(hello, nullptr);
    const auto hello_workspace = cppapplab::create_workspace(
        *hello, profile.home / "cppapplab-work" / "w1.hello-world");
    write_source(hello_workspace.root,
                 "#include <iostream>\n"
                 "int main() { std::cout << \"Hello, CppAppLab!\\n\"; return 0; }\n");
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "check", hello_workspace.root.u8string()}), 0)
            << capture.str();
    }
    ASSERT_TRUE(cppapplab::has_current_pass("w1.hello-world"));

    const auto* calculator = cppapplab::find_task("w1.calculator");
    ASSERT_NE(calculator, nullptr);
    const auto calculator_workspace = cppapplab::create_workspace(
        *calculator, profile.home / "cppapplab-work" / "w1.calculator");
    const auto reference = cppapplab::data_root() / "labs" / "w1" / "calculator" / "reference";
    for (const auto& relative : calculator->source_files) {
        const auto source = reference / relative;
        const auto destination = calculator_workspace.root / relative;
        fs::create_directories(destination.parent_path());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    }

    std::string check_output;
    int check_result = -1;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", calculator_workspace.root.u8string()});
        check_output = capture.str();
    }
    ASSERT_EQ(check_result, 0) << check_output;
    EXPECT_NE(check_output.find("Result: PASS"), std::string::npos);
    EXPECT_NE(check_output.find("Acceptance: " + std::to_string(calculator->expected_test_count) +
                                "/" + std::to_string(calculator->expected_test_count)),
              std::string::npos);
    EXPECT_TRUE(cppapplab::has_current_pass("w1.calculator"));

    // Continue through the real W2-W4 platform-owned CMake/GoogleTest fixtures.
    // Each task receives only its declared reference sources; the shared learning
    // engine still stages them, invokes CMake/CTest, and writes the formal evidence.
    for (const std::string task_id : {"w2.todo-cli", "w3.task-manager", "w4.quality-gate"}) {
        const auto* task = cppapplab::find_task(task_id);
        ASSERT_NE(task, nullptr) << task_id;
        const auto workspace = cppapplab::create_workspace(
            *task, profile.home / "cppapplab-work" / task->id);
        const auto reference = cppapplab::data_root() /
                               task->learner_template.parent_path() / "reference";
        for (const auto& relative : task->source_files) {
            const auto source = reference / relative;
            const auto destination = workspace.root / relative;
            fs::create_directories(destination.parent_path());
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        }

        std::string output;
        int result = -1;
        {
            StdoutCapture capture;
            result = invoke({"cppapplab", "check", workspace.root.u8string()});
            output = capture.str();
        }
        ASSERT_EQ(result, 0) << task_id << "\n" << output;
        EXPECT_NE(output.find("Result: PASS"), std::string::npos) << task_id;
        EXPECT_NE(output.find("Acceptance: " + std::to_string(task->expected_test_count) +
                              "/" + std::to_string(task->expected_test_count)),
                  std::string::npos) << task_id;
        EXPECT_TRUE(cppapplab::has_current_pass(task_id)) << task_id;
    }
}

TEST(NativeLearningWorkflow, WrongOutputCannotUnlockCalculator) {
    Scratch scratch;
    IsolatedProfile profile(scratch.root());

    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
    }
    const auto workspace = profile.home / "cppapplab-work" / "w1.hello-world";
    write_source(workspace,
                 "#include <iostream>\n"
                 "int main() { std::cout << \"not the required output\\n\"; return 0; }\n");

    std::string check_output;
    int check_result = -1;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", workspace.u8string()});
        check_output = capture.str();
    }
    EXPECT_EQ(check_result, 1) << check_output;
    EXPECT_NE(check_output.find("Result: FAIL"), std::string::npos);
    EXPECT_FALSE(cppapplab::has_current_pass("w1.hello-world"));

    const auto calculator = scratch.root() / L"locked calculator workspace";
    EXPECT_EQ(invoke({"cppapplab", "start", "w1.calculator", calculator.u8string()}), 2);
    EXPECT_FALSE(fs::exists(calculator));
}

TEST(NativeLearningWorkflow, LessonAndHintsReadFromUnicodeWorkspace) {
    Scratch scratch;
    IsolatedProfile profile(scratch.root());

    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "learn"}), 0);
    }
    const auto workspace = profile.home / "cppapplab-work" / "w1.hello-world";

    std::string lesson;
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "lesson", workspace.u8string()}), 0);
        lesson = capture.str();
    }
    EXPECT_NE(lesson.find("First C++ Program"), std::string::npos);

    std::string hint;
    {
        StdoutCapture capture;
        ASSERT_EQ(invoke({"cppapplab", "hint", workspace.u8string(), "1"}), 0);
        hint = capture.str();
    }
    EXPECT_FALSE(hint.empty());
}

}  // namespace
