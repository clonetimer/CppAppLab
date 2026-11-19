#include "cppapplab/process.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::filesystem::path child_executable() {
    const char* configured = std::getenv("CPPAPPLAB_PROCESS_TEST_CHILD");
    if (configured == nullptr || configured[0] == '\0') {
        throw std::runtime_error("CPPAPPLAB_PROCESS_TEST_CHILD is not set by CTest.");
    }
    return std::filesystem::path(configured);
}

std::vector<std::string> child_command(std::initializer_list<std::string> arguments) {
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1U);
    command.push_back(child_executable().string());
    command.insert(command.end(), arguments.begin(), arguments.end());
    return command;
}

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("CppAppLab process runner test with spaces " + std::to_string(stamp));
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

}  // namespace

TEST(ProcessRunnerTest, CapturesStandardOutput) {
    const auto result = cppapplab::run_process(
        child_command({"emit", "CppAppLab"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, "CppAppLab\n");
}

TEST(ProcessRunnerTest, CapturesStandardErrorTogetherWithStandardOutput) {
    const auto result = cppapplab::run_process(
        child_command({"emit-stderr", "child diagnostic"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.output, "child diagnostic\n");
}

TEST(ProcessRunnerTest, UsesNullInputWhenNoInputIsProvided) {
    const auto result = cppapplab::run_process(
        child_command({"copy-stdin"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_TRUE(result.output.empty());
}

TEST(ProcessRunnerTest, PassesBoundedStandardInputToChild) {
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const auto result = cppapplab::run_process(
        child_command({"copy-stdin"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024, request);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, request);
}

TEST(ProcessRunnerTest, PreservesBinaryStandardInput) {
    const std::string payload("A\\0B\\r\\nC", 6U);
    const auto result = cppapplab::run_process(
        child_command({"copy-stdin"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024, payload);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, payload);
}

TEST(ProcessRunnerTest, RejectsOversizedStandardInputBeforeStartingChild) {
    const std::string oversized(256U * 1024U + 1U, 'x');
    EXPECT_THROW(
        (void)cppapplab::run_process(child_command({"copy-stdin"}),
                                     std::filesystem::current_path(),
                                     std::chrono::seconds(2), 1024, oversized),
        std::invalid_argument);
}

TEST(ProcessRunnerTest, EnforcesTimeLimit) {
    const auto result = cppapplab::run_process(
        child_command({"sleep", "2000"}), std::filesystem::current_path(),
        std::chrono::milliseconds(100), 1024);

    EXPECT_TRUE(result.timed_out);
}

TEST(ProcessRunnerTest, KillsDescendantsWithTheTimedOutProcessTree) {
    const auto started = std::chrono::steady_clock::now();
    const auto result = cppapplab::run_process(
        child_command({"spawn-child", "5000"}), std::filesystem::current_path(),
        std::chrono::milliseconds(150), 1024);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(result.timed_out);
    EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST(ProcessRunnerTest, BoundsCapturedOutputWhileContinuingToDrainThePipe) {
    const auto result = cppapplab::run_process(
        child_command({"write-repeat", "1048576"}), std::filesystem::current_path(),
        std::chrono::seconds(5), 4);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_TRUE(result.output_truncated);
    EXPECT_EQ(result.output, "xxxx");
}

TEST(ProcessRunnerTest, PreservesArgumentsContainingSpacesQuotesAndBackslashes) {
    const auto result = cppapplab::run_process(
        child_command({"join-args", "a path with spaces", "quote\"value", "trailing\\\\"}),
        std::filesystem::current_path(), std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.output,
              "[a path with spaces]\n"
              "[quote\"value]\n"
              "[trailing\\\\]\n");
}

TEST(ProcessRunnerTest, UsesRequestedWorkingDirectoryContainingSpaces) {
    TemporaryDirectory temporary;
    const auto result = cppapplab::run_process(
        child_command({"write-cwd-marker"}), temporary.path(), std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        temporary.path() / "process-runner-child.marker"));
}

TEST(ProcessRunnerTest, ReturnsConventionalCodeWhenWorkingDirectoryDoesNotExist) {
    TemporaryDirectory temporary;
    const auto missing = temporary.path() / "missing-working-directory";
    const auto result = cppapplab::run_process(
        child_command({"emit", "not reached"}), missing, std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 126);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output.empty());
}

TEST(ProcessRunnerTest, ReturnsTheChildExitCode) {
    const auto result = cppapplab::run_process(
        child_command({"exit", "6"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 6);
    EXPECT_FALSE(result.timed_out);
}

TEST(ProcessRunnerTest, ReturnsConventionalCodeWhenExecutableDoesNotExist) {
    const auto result = cppapplab::run_process(
        {"cppapplab-executable-that-does-not-exist-20261119"}, std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 127);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output.empty());
}
