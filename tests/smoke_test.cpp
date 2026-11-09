#include "cppapplab/app.hpp"

#include <gtest/gtest.h>

#include <string_view>

TEST(CppAppLabSmokeTest, ExposesProjectVersion) {
    EXPECT_EQ(cppapplab::version(), "0.1.0");
}

TEST(CppAppLabSmokeTest, HelpTextContainsSupportedCommands) {
    const std::string_view help = cppapplab::usage();

    EXPECT_NE(help.find("cppapplab --help"), std::string_view::npos);
    EXPECT_NE(help.find("cppapplab --version"), std::string_view::npos);
}
