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

TEST(ProcessRunnerTest, EnforcesTimeLimit) {
    const auto result = cppapplab::run_process(
        {"/bin/sleep", "2"},
        std::filesystem::current_path(),
        std::chrono::milliseconds(100),
        1024
    );

    EXPECT_TRUE(result.timed_out);
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
