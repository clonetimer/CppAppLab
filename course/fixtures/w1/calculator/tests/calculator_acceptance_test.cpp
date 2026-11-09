#include "calculator.hpp"

#include <gtest/gtest.h>

TEST(CalculatorAcceptanceTest, AddsPositiveOperands) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Add, 2.0, 3.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::None);
    EXPECT_DOUBLE_EQ(result.value, 5.0);
}

TEST(CalculatorAcceptanceTest, SubtractsOperands) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Subtract, 9.0, 4.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::None);
    EXPECT_DOUBLE_EQ(result.value, 5.0);
}

TEST(CalculatorAcceptanceTest, MultipliesOperands) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Multiply, -3.0, 4.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::None);
    EXPECT_DOUBLE_EQ(result.value, -12.0);
}

TEST(CalculatorAcceptanceTest, DividesOperands) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Divide, 7.5, 2.5);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::None);
    EXPECT_DOUBLE_EQ(result.value, 3.0);
}

TEST(CalculatorAcceptanceTest, ReportsDivisionByZero) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Divide, 1.0, 0.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::DivisionByZero);
}

TEST(CalculatorAcceptanceTest, HandlesZeroAndNegativeOperands) {
    const auto result =
        cppapplab::calculator::calculate(cppapplab::calculator::Operation::Add, -2.5, 0.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::None);
    EXPECT_DOUBLE_EQ(result.value, -2.5);
}

TEST(CalculatorAcceptanceTest, RejectsAnInvalidOperationValue) {
    const auto invalid_operation = static_cast<cppapplab::calculator::Operation>(99);
    const auto result = cppapplab::calculator::calculate(invalid_operation, 1.0, 2.0);

    EXPECT_EQ(result.error, cppapplab::calculator::Error::InvalidOperation);
}
