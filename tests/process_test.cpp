#include "cppapplab/process.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

TEST(ProcessRunnerTest, CapturesStandardOutput) {
    const auto result = cppapplab::run_process(
        {"/bin/echo", "CppAppLab"},
        std::filesystem::current_path(),
        std::chrono::seconds(2),
        1024
    );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.output_truncated);
    EXPECT_EQ(result.output, "CppAppLab\n");
}

TEST(ProcessRunnerTest, UsesNullInputWhenNoInputIsProvided) {
    const auto result = cppapplab::run_process(
        {"/bin/cat"},
        std::filesystem::current_path(),
        std::chrono::seconds(2),
        1024
    );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.timed_out);
    EXPECT_TRUE(result.output.empty());
}

TEST(ProcessRunnerTest, EnforcesTimeLimit) {
    const auto result = cppapplab::run_process(
        {"/bin/sleep", "2"},
        std::filesystem::current_path(),
        std::chrono::milliseconds(100),
        1024
    );

    EXPECT_TRUE(result.timed_out);
}

TEST(ProcessRunnerTest, KillsDescendantsWithTheTimedOutProcessGroup) {
    const auto started = std::chrono::steady_clock::now();
    const auto result = cppapplab::run_process(
        {"/bin/sh", "-c", "sleep 5 & wait"},
        std::filesystem::current_path(),
        std::chrono::milliseconds(100),
        1024
    );
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(result.timed_out);
    EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST(ProcessRunnerTest, BoundsCapturedOutput) {
    const auto result = cppapplab::run_process(
        {"/bin/echo", "this-output-is-longer-than-the-limit"},
        std::filesystem::current_path(),
        std::chrono::seconds(2),
        4
    );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.output_truncated);
    EXPECT_EQ(result.output.size(), 4U);
}
