#include "contract.hpp"
#include <array>
#include <limits>
#include <gtest/gtest.h>

using modern::summarize;
TEST(Project, Empty) {
    EXPECT_FALSE(summarize({}));
}
TEST(Project, Nominal) {
    const std::array<double, 3> samples{10, 20, 30};
    const auto result = summarize(samples);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->count, 3U);
    EXPECT_DOUBLE_EQ(result->mean, 20);
    EXPECT_DOUBLE_EQ(result->minimum, 10);
    EXPECT_DOUBLE_EQ(result->maximum, 30);
}
TEST(Project, ClosedBoundaries) {
    const std::array<double, 4> samples{-1, 0, 100, 101};
    const auto result = summarize(samples);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->count, 2U);
    EXPECT_DOUBLE_EQ(result->mean, 50);
    EXPECT_DOUBLE_EQ(result->minimum, 0);
    EXPECT_DOUBLE_EQ(result->maximum, 100);
}
TEST(Project, NoValidSamples) {
    const std::array<double, 3> samples{-1, 101, -100};
    EXPECT_FALSE(summarize(samples));
}
TEST(Project, NonFinite) {
    const std::array<double, 4> samples{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), 42};
    const auto result = summarize(samples);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->count, 1U);
    EXPECT_DOUBLE_EQ(result->mean, 42);
    EXPECT_DOUBLE_EQ(result->minimum, 42);
    EXPECT_DOUBLE_EQ(result->maximum, 42);
}
TEST(Project, Subspan) {
    const std::array<double, 5> samples{100, 1, 4, 7, 100};
    const auto result = summarize(std::span<const double>(samples).subspan(1, 3));
    ASSERT_TRUE(result);
    EXPECT_EQ(result->count, 3U);
    EXPECT_DOUBLE_EQ(result->mean, 4);
    EXPECT_DOUBLE_EQ(result->minimum, 1);
    EXPECT_DOUBLE_EQ(result->maximum, 7);
}
TEST(Project, DoesNotModifyInput) {
    std::array<double, 4> samples{60, 0, 1, 4};
    const auto before = samples;
    const auto result = summarize(samples);
    ASSERT_TRUE(result);
    EXPECT_EQ(samples, before);
    EXPECT_DOUBLE_EQ(result->mean, 16.25);
}
