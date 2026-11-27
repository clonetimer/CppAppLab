#include "cppapplab/process.hpp"
#include "cppapplab_windows_w1_config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;
constexpr std::string_view kExpected = "Hello, CppAppLab!\n";

class ScratchDirectory {
   public:
    ScratchDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab native w1 " + std::to_string(stamp));
        if (!std::filesystem::create_directory(path_)) {
            throw std::runtime_error("Cannot create isolated W1 directory");
        }
    }
    ~ScratchDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

   private:
    std::filesystem::path path_;
};

void write_source(const std::filesystem::path& path, std::string_view source) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << source;
    stream.close();
    if (!stream) {
        throw std::runtime_error("Cannot write isolated W1 source");
    }
}

// This is a text-contract adapter only. The shared run_process contract remains
// byte-preserving (including binary stdin and stdout).
[[nodiscard]] bool matches_hello(const cppapplab::ProcessResult& result) {
    if (result.exit_code != 0 || result.timed_out || result.output_truncated) {
        return false;
    }
    std::string logical;
    for (std::size_t i = 0; i < result.output.size(); ++i) {
        if (result.output[i] == '\r') {
            if (i + 1U >= result.output.size() || result.output[i + 1U] != '\n') {
                return false;
            }
            ++i;
            logical.push_back('\n');
        } else {
            logical.push_back(result.output[i]);
        }
    }
    return logical == kExpected;
}

[[nodiscard]] cppapplab::ProcessResult compile_hello(const std::filesystem::path& root) {
    return cppapplab::run_process(
        {CPPAPPLAB_NATIVE_GXX, "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-g", "main.cpp", "-o", "hello.exe"},
        root, 30s, 64U * 1024U);
}

[[nodiscard]] cppapplab::ProcessResult run_hello(const std::filesystem::path& root) {
    return cppapplab::run_process({(root / "hello.exe").u8string()}, root, 5s, 4096U);
}

TEST(NativeW1Foundation, CompilesActualStarterAndChecksFixedTextAfterRepair) {
    ScratchDirectory directory;
    std::filesystem::copy_file(std::filesystem::u8path(CPPAPPLAB_NATIVE_HELLO_STARTER),
                               directory.path() / "main.cpp");
    auto build = compile_hello(directory.path());
    ASSERT_EQ(build.exit_code, 0) << build.output;
    ASSERT_FALSE(build.timed_out);
    auto starter = run_hello(directory.path());
    ASSERT_EQ(starter.exit_code, 0) << starter.output;
    EXPECT_TRUE(starter.output.empty());
    EXPECT_FALSE(matches_hello(starter));

    write_source(directory.path() / "main.cpp",
                 "#include <iostream>\nint main() { std::cout << \"Hello, CppAppLab!\\n\"; return 0; }\n");
    build = compile_hello(directory.path());
    ASSERT_EQ(build.exit_code, 0) << build.output;
    ASSERT_FALSE(build.timed_out);
    const auto corrected = run_hello(directory.path());
    EXPECT_TRUE(matches_hello(corrected)) << corrected.output;
    EXPECT_TRUE(corrected.output == "Hello, CppAppLab!\n" ||
                corrected.output == "Hello, CppAppLab!\r\n");
}

TEST(NativeW1Foundation, RejectsCompilationErrorsAndIncorrectOutput) {
    ScratchDirectory directory;
    write_source(directory.path() / "main.cpp", "int main() { return missing_name; }\n");
    const auto invalid = compile_hello(directory.path());
    EXPECT_NE(invalid.exit_code, 0);
    EXPECT_FALSE(invalid.timed_out);
    EXPECT_NE(invalid.output.find("missing_name"), std::string::npos);

    write_source(directory.path() / "main.cpp",
                 "#include <iostream>\nint main() { std::cout << \"Hello, CppAppLab!\"; return 0; }\n");
    auto build = compile_hello(directory.path());
    ASSERT_EQ(build.exit_code, 0) << build.output;
    EXPECT_FALSE(matches_hello(run_hello(directory.path())));
}

TEST(NativeW1Foundation, TextAdapterNeverWeakensOtherAcceptanceChecks) {
    cppapplab::ProcessResult result;
    result.exit_code = 0;
    result.output = "Hello, CppAppLab!\r\n";
    EXPECT_TRUE(matches_hello(result));
    result.output = "Hello, CppAppLab!\n";
    EXPECT_TRUE(matches_hello(result));
    result.output = "Hello, CppAppLab!\r";
    EXPECT_FALSE(matches_hello(result));
    result.output = "Hello, CppAppLab!\nextra";
    EXPECT_FALSE(matches_hello(result));
    result.output = "Hello, CppAppLab!\n";
    result.exit_code = 1;
    EXPECT_FALSE(matches_hello(result));
    result.exit_code = 0;
    result.output_truncated = true;
    EXPECT_FALSE(matches_hello(result));
    result.output_truncated = false;
    result.timed_out = true;
    EXPECT_FALSE(matches_hello(result));
}

}  // namespace
