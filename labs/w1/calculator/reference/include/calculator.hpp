#pragma once

namespace cppapplab::calculator {

enum class Operation {
    Add,
    Subtract,
    Multiply,
    Divide
};

enum class Error {
    None,
    DivisionByZero,
    InvalidOperation
};

struct Result {
    double value{};
    Error error{Error::None};
};

[[nodiscard]] Result calculate(
    Operation operation,
    double left,
    double right
) noexcept;

}  // namespace cppapplab::calculator
