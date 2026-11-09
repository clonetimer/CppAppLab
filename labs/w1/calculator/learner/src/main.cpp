#include "calculator.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool parse_number(std::string_view text, double& value) {
    const std::string owned_text{text};
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(owned_text.c_str(), &end);

    if (end == owned_text.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

bool parse_operation(std::string_view text, cppapplab::calculator::Operation& operation) {
    using cppapplab::calculator::Operation;

    if (text == "add") {
        operation = Operation::Add;
    } else if (text == "subtract") {
        operation = Operation::Subtract;
    } else if (text == "multiply") {
        operation = Operation::Multiply;
    } else if (text == "divide") {
        operation = Operation::Divide;
    } else {
        return false;
    }

    return true;
}

void print_usage() {
    std::cerr << "Usage: calculator <add|subtract|multiply|divide> <left> <right>\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage();
        return 2;
    }

    cppapplab::calculator::Operation operation{};
    double left = 0.0;
    double right = 0.0;

    if (!parse_operation(argv[1], operation) || !parse_number(argv[2], left) ||
        !parse_number(argv[3], right)) {
        std::cerr << "error: invalid operation or number\n";
        print_usage();
        return 2;
    }

    const auto result = cppapplab::calculator::calculate(operation, left, right);
    if (result.error == cppapplab::calculator::Error::DivisionByZero) {
        std::cerr << "error: division by zero\n";
        return 3;
    }
    if (result.error != cppapplab::calculator::Error::None) {
        std::cerr << "error: invalid operation\n";
        return 3;
    }

    std::cout << std::setprecision(12) << result.value << '\n';
    return 0;
}
