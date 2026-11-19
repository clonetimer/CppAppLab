#include "cppapplab/process.hpp"
#include "cppapplab_process_test_config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string& child_executable() {
    static const std::string path = CPPAPPLAB_PROCESS_TEST_CHILD;
    return path;
}

std::vector<std::string> child_arguments(std::initializer_list<std::string> arguments) {
    std::vector<std::string> result{child_executable()};
    result.insert(result.end(), arguments.begin(), arguments.end());
    return result;
}

}  // namespace

TEST(ProcessRunnerTest, CapturesStandardOutput) {
    const auto result = cppapplab::run_process(
        child_arguments({"emit", "CppAppLab"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, "CppAppLab\n");
}

TEST(ProcessRunnerTest, PreservesArgumentsContainingSpacesQuotesAndTrailingBackslashes) {
    const std::string argument = R"(value with spaces and "quotes" and trailing\)";
    const auto result = cppapplab::run_process(
        child_arguments({"emit", argument}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.output, argument + "\n");
}

TEST(ProcessRunnerTest, PreservesEmptyArguments) {
    const auto result = cppapplab::run_process(
        child_arguments({"emit", ""}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.output, "\n");
}

TEST(ProcessRunnerTest, UsesClosedStandardInputWhenNoInputIsProvided) {
    const auto result = cppapplab::run_process(
        child_arguments({"copy-stdin"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_TRUE(result.output.empty());
}

TEST(ProcessRunnerTest, PassesBoundedStandardInputToChild) {
    const std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const auto result = cppapplab::run_process(
        child_arguments({"copy-stdin"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024, request);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, request);
}

TEST(ProcessRunnerTest, RejectsOversizedStandardInputBeforeStartingChild) {
    const std::string oversized(256U * 1024U + 1U, 'x');
    EXPECT_THROW(
        (void)cppapplab::run_process(child_arguments({"copy-stdin"}),
                                     std::filesystem::current_path(),
                                     std::chrono::seconds(2), 1024, oversized),
        std::invalid_argument);
}

TEST(ProcessRunnerTest, EnforcesTimeLimit) {
    const auto result = cppapplab::run_process(
        child_arguments({"sleep", "2000"}), std::filesystem::current_path(),
        std::chrono::milliseconds(100), 1024);

    EXPECT_TRUE(result.timed_out);
}

TEST(ProcessRunnerTest, KillsDescendantsWithTheTimedOutProcessTree) {
    const auto started = std::chrono::steady_clock::now();
    const auto result = cppapplab::run_process(
        child_arguments({"spawn-wait", "5000"}), std::filesystem::current_path(),
        std::chrono::milliseconds(100), 1024);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(result.timed_out);
    EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST(ProcessRunnerTest, PreservesNonzeroExitCodes) {
    const auto result = cppapplab::run_process(
        child_arguments({"exit", "6"}), std::filesystem::current_path(),
        std::chrono::seconds(2), 1024);

    EXPECT_EQ(result.exit_code, 6);
    EXPECT_FALSE(result.timed_out);
}

TEST(ProcessRunnerTest, BoundsCapturedOutput) {
    const auto result = cppapplab::run_process(
        child_arguments({"emit", "this-output-is-longer-than-the-limit"}),
        std::filesystem::current_path(), std::chrono::seconds(2), 4);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.output_truncated);
    EXPECT_EQ(result.output.size(), 4U);
}
