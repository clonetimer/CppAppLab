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
                ("cppapplab-http-server-workflow-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& path() const noexcept { return path_; }

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

}  // namespace

TEST(HttpServerWorkflowTest, CatalogPreservesPrerequisiteAndAcceptanceContract) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 5U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 29U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.router");
}

TEST(HttpServerWorkflowTest, PlatformCheckUsesFixedFixtureAndReferenceImplementation) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace =
        cppapplab::create_workspace(*task, temporary.path() / "http-server");
    const auto reference = cppapplab::data_root() / "labs/w8/http-server/reference";
    for (const auto& relative_path : task->source_files) {
        std::filesystem::copy_file(reference / relative_path, workspace.root / relative_path,
                                   std::filesystem::copy_options::overwrite_existing);
    }
    {
        std::ofstream build_file(workspace.root / "CMakeLists.txt", std::ios::trunc);
        ASSERT_TRUE(build_file.good());
        build_file << "message(FATAL_ERROR \"Learner CMakeLists must not run\")\n";
    }

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
    EXPECT_EQ(evidence->tests_passed, 28U) << diagnostic;
    EXPECT_EQ(evidence->tests_total, 28U) << diagnostic;
}
