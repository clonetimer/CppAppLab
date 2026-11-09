#include "calculator.hpp"

namespace cppapplab::calculator {

Result calculate(Operation operation, double left, double right) noexcept {
    switch (operation) {
        case Operation::Add:
            return {left + right, Error::None};
        case Operation::Subtract:
            return {left - right, Error::None};
        case Operation::Multiply:
            return {left * right, Error::None};
        case Operation::Divide:
            if (right == 0.0) {
                return {0.0, Error::DivisionByZero};
            }
            return {left / right, Error::None};
    }

    return {0.0, Error::InvalidOperation};
}

}  // namespace cppapplab::calculator
