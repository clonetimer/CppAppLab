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
    const std::filesystem::path& path() const noexcept {
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

}  // namespace

TEST(HttpServerWorkflowTest, CatalogPreservesPrerequisiteAndAcceptanceContract) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->week, 8U);
    EXPECT_EQ(task->order, 5U);
    EXPECT_EQ(task->acceptance_kind, cppapplab::AcceptanceKind::CMakeGoogleTest);
    EXPECT_EQ(task->source_files.size(), 4U);
    EXPECT_EQ(task->expected_test_count, 34U);
    ASSERT_EQ(task->prerequisites.size(), 1U);
    EXPECT_EQ(task->prerequisites.front(), "w8.router");
}

TEST(HttpServerWorkflowTest, PlatformCheckUsesFixedFixtureAndReferenceImplementation) {
    const cppapplab::Task* task = cppapplab::find_task("w8.http-server");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "http-server");
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
    EXPECT_EQ(evidence->tests_passed, 34U) << diagnostic;
    EXPECT_EQ(evidence->tests_total, 34U) << diagnostic;
}

TEST(HttpServerWorkflowTest, D6AdvancedTasksUseLearnerSourcesInFixedAcceptance) {
    const std::vector<std::string> task_ids{
        "w8.http-connection-policy", "w8.http-keep-alive",    "w8.http-connection-guards",
        "w8.http-graceful-shutdown", "w8.http-observability", "w8.http-operational-load",
    };

    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");

    for (const std::string& task_id : task_ids) {
        const cppapplab::Task* task = cppapplab::find_task(task_id);
        ASSERT_NE(task, nullptr) << task_id;

        const auto workspace = cppapplab::create_workspace(*task, temporary.path() / task_id);
        std::filesystem::path reference;
        if (task_id == "w8.http-connection-policy") {
            reference = cppapplab::data_root() / "labs/w8/router/reference";
        } else if (task_id == "w8.http-operational-load") {
            reference = cppapplab::data_root() / "labs/w8/http-load-probe/reference";
        } else {
            reference = cppapplab::data_root() / "labs/w8/http-server/reference";
        }

        for (const auto& relative_path : task->source_files) {
            std::filesystem::copy_file(reference / relative_path, workspace.root / relative_path,
                                       std::filesystem::copy_options::overwrite_existing);
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
        EXPECT_EQ(result, 0) << task_id << '\n' << diagnostic;
        const auto evidence = cppapplab::read_evidence(workspace.root);
        ASSERT_TRUE(evidence.has_value()) << task_id << '\n' << diagnostic;
        EXPECT_TRUE(evidence->passed) << task_id << '\n' << diagnostic;
        EXPECT_EQ(evidence->tests_passed, task->expected_test_count) << task_id;
        EXPECT_EQ(evidence->tests_total, task->expected_test_count) << task_id;
    }
}

TEST(HttpServerWorkflowTest, W9AdmissionStarterFailsAndReferencePassesFixedContract) {
    const cppapplab::Task* task = cppapplab::find_task("w9.http-hardening");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "http-admission");
    auto check = [&]() {
        std::vector<std::string> arguments{"cppapplab", "check", workspace.root.string()};
        std::vector<char*> argv;
        for (auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    };

    EXPECT_NE(check(), 0);
    const auto failed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(failed.has_value());
    EXPECT_FALSE(failed->passed);
    const auto reference = cppapplab::data_root() / "labs/w9/http-hardening/reference";
    std::filesystem::copy_file(reference / "src/http_hardening.cpp",
                               workspace.root / "src/http_hardening.cpp",
                               std::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream untrusted(workspace.root / "include/http_hardening.hpp", std::ios::trunc);
        ASSERT_TRUE(untrusted.good());
        untrusted << "#error Learner header must not define the fixed contract\n";
    }
    EXPECT_EQ(check(), 0);
    const auto passed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(passed.has_value());
    EXPECT_TRUE(passed->passed);
    EXPECT_EQ(passed->tests_total, task->expected_test_count);
    EXPECT_EQ(passed->tests_passed, task->expected_test_count);
}

TEST(HttpServerWorkflowTest, W9FuzzOracleStarterFailsAndReferencePassesFixedContract) {
    const cppapplab::Task* task = cppapplab::find_task("w9.http-fuzzing");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "fuzz-oracle");
    auto check = [&]() {
        std::vector<std::string> arguments{"cppapplab", "check", workspace.root.string()};
        std::vector<char*> argv;
        for (auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    };

    EXPECT_NE(check(), 0);
    const auto failed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(failed.has_value());
    EXPECT_FALSE(failed->passed);
    const auto reference = cppapplab::data_root() / "labs/w9/http-fuzzing/reference";
    std::filesystem::copy_file(reference / "src/admission_invariants.cpp",
                               workspace.root / "src/admission_invariants.cpp",
                               std::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream untrusted(workspace.root / "include/admission_invariants.hpp", std::ios::trunc);
        ASSERT_TRUE(untrusted.good());
        untrusted << "#error Learner header must not define the fixed contract\n";
    }
    EXPECT_EQ(check(), 0);
    const auto passed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(passed.has_value());
    EXPECT_TRUE(passed->passed);
    EXPECT_EQ(passed->tests_total, task->expected_test_count);
    EXPECT_EQ(passed->tests_passed, task->expected_test_count);
}

TEST(HttpServerWorkflowTest, W10TlsContextStarterFailsAndReferencePassesFixedContract) {
    const cppapplab::Task* task = cppapplab::find_task("w10.tls-context");
    ASSERT_NE(task, nullptr);
    TemporaryDirectory temporary;
    ScopedEnvironment state_directory("CPPAPPLAB_STATE_DIR", temporary.path() / "state");
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "tls-context");
    auto check = [&]() {
        std::vector<std::string> arguments{"cppapplab", "check", workspace.root.string()};
        std::vector<char*> argv;
        for (auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
    };

    EXPECT_NE(check(), 0);
    const auto failed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(failed.has_value());
    EXPECT_FALSE(failed->passed);
    const auto reference = cppapplab::data_root() / "labs/w10/tls-context/reference";
    std::filesystem::copy_file(reference / "src/tls_context.cpp",
                               workspace.root / "src/tls_context.cpp",
                               std::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream untrusted(workspace.root / "include/tls_context.hpp", std::ios::trunc);
        ASSERT_TRUE(untrusted.good());
        untrusted << "#error Learner header must not define the fixed contract\n";
    }
    EXPECT_EQ(check(), 0);
    const auto passed = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(passed.has_value());
    EXPECT_TRUE(passed->passed);
    EXPECT_EQ(passed->tests_total, task->expected_test_count);
    EXPECT_EQ(passed->tests_passed, task->expected_test_count);
}
