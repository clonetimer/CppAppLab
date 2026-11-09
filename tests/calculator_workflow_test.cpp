#include "cppapplab/learning.hpp"
#include "cppapplab/task.hpp"
#include "cppapplab/workspace.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-calculator-test-" + std::to_string(stamp));
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

class StdoutCapture {
   public:
    StdoutCapture() : previous_(std::cout.rdbuf(stream_.rdbuf())) {}

    ~StdoutCapture() {
        std::cout.rdbuf(previous_);
    }

    [[nodiscard]] std::string str() const {
        return stream_.str();
    }

   private:
    std::ostringstream stream_;
    std::streambuf* previous_;
};

int invoke(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    return cppapplab::run_cli(static_cast<int>(argv.size()), argv.data());
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open test file: " + path.string());
    }
    output << content;
    output.flush();
    if (!output) {
        throw std::runtime_error("Cannot write test file: " + path.string());
    }
}

}  // namespace

TEST(CalculatorWorkflowTest, FixedCMakeAndGoogleTestFixtureChecksLearnerImplementation) {
    const cppapplab::Task* task = cppapplab::find_task("w1.calculator");
    ASSERT_NE(task, nullptr);

    TemporaryDirectory temporary;
    const auto workspace = cppapplab::create_workspace(*task, temporary.path() / "calculator");

    write_file(workspace.root / "src/calculator.cpp",
               "#include \"calculator.hpp\"\n"
               "namespace cppapplab::calculator {\n"
               "Result calculate(Operation operation, double left, double right) noexcept {\n"
               "    switch (operation) {\n"
               "        case Operation::Add: return {left + right, Error::None};\n"
               "        case Operation::Subtract: return {left - right, Error::None};\n"
               "        case Operation::Multiply: return {left * right, Error::None};\n"
               "        case Operation::Divide:\n"
               "            if (right == 0.0) return {0.0, Error::DivisionByZero};\n"
               "            return {left / right, Error::None};\n"
               "    }\n"
               "    return {0.0, Error::InvalidOperation};\n"
               "}\n"
               "}\n");

    // This file is intentionally invalid: the platform must use its own fixture,
    // not a learner-controlled CMakeLists.txt.
    write_file(workspace.root / "CMakeLists.txt",
               "message(FATAL_ERROR \"Learner CMakeLists must not be executed by platform "
               "acceptance\")\n");

    int check_result = -1;
    std::string check_output;
    {
        StdoutCapture capture;
        check_result = invoke({"cppapplab", "check", workspace.root.string()});
        check_output = capture.str();
    }

    if (check_result != 0) {
        std::ifstream log_file(workspace.root / ".cppapplab/last-check.log", std::ios::binary);
        if (log_file) {
            check_output += "\n--- acceptance log ---\n";
            check_output.append(std::istreambuf_iterator<char>(log_file),
                                std::istreambuf_iterator<char>());
        }
    }

    EXPECT_EQ(check_result, 0) << check_output;
    EXPECT_NE(check_output.find("Result: PASS"), std::string::npos);
    EXPECT_NE(check_output.find("Acceptance: 9/9"), std::string::npos);

    const auto evidence = cppapplab::read_evidence(workspace.root);
    ASSERT_TRUE(evidence.has_value());
    EXPECT_TRUE(evidence->passed);
    EXPECT_EQ(evidence->tests_passed, 9U);
    EXPECT_EQ(evidence->tests_total, 9U);
}
