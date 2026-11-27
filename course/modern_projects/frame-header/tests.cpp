#include "contract.hpp"
#include <array>
#include <gtest/gtest.h>

using modern::decode_header;
using modern::FrameError;
using Bytes = std::array<std::uint8_t, 8>;
TEST(Project, Nominal) {
    const Bytes b{0xa5,1,0,42,1,2,3,4};
    const auto result = decode_header(b);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->payload_length, 42U);
    EXPECT_EQ(result->sequence, 0x01020304U);
}
TEST(Project, ZeroAndMaximumSequence) {
    const Bytes b{0xa5,1,0,0,255,255,255,255};
    const auto result = decode_header(b);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->payload_length, 0U);
    EXPECT_EQ(result->sequence, 0xffffffffU);
}
TEST(Project, MaxLength) {
    const Bytes b{0xa5,1,4,0,0,0,0,42};
    const auto result = decode_header(b);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->payload_length, 1024U);
    EXPECT_EQ(result->sequence, 42U);
}
TEST(Project, ShortAndLong) {
    const std::array<std::uint8_t, 7> short_b{};
    const std::array<std::uint8_t, 9> long_b{};
    for (auto b : {std::span<const std::uint8_t>(short_b), std::span<const std::uint8_t>(long_b)}) {
        const auto result = decode_header(b);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), FrameError::Size);
    }
}
TEST(Project, Magic) {
    const Bytes b{0,1,0,0,0,0,0,0};
    const auto result = decode_header(b);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), FrameError::Magic);
}
TEST(Project, Version) {
    const Bytes b{0xa5,2,0,0,0,0,0,0};
    const auto result = decode_header(b);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), FrameError::Version);
}
TEST(Project, OversizedPayload) {
    const Bytes b{0xa5,1,4,1,0,0,0,0};
    const auto result = decode_header(b);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), FrameError::Length);
}
TEST(Project, ErrorPrecedence) {
    const Bytes bad_magic{0,2,255,255,0,0,0,0};
    const Bytes bad_version{0xa5,2,255,255,0,0,0,0};
    const auto a = decode_header(bad_magic);
    const auto b = decode_header(bad_version);
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);
    EXPECT_EQ(a.error(), FrameError::Magic);
    EXPECT_EQ(b.error(), FrameError::Version);
}
