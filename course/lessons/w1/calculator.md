# W1 · Multi-file Calculator

## Goal

Extend the first C++ program into a multi-file application. Separate a public interface from its implementation, compile multiple translation units, and handle an arithmetic error explicitly.

## Your task

Implement the function declared in:

\`\`\`text
include/calculator.hpp
\`\`\`

in:

\`\`\`text
src/calculator.cpp
\`\`\`

The command-line front end in \`src/main.cpp\` is provided. It parses the operation and operands, calls your \`calculate\` function, and prints the result or a diagnostic.

Implement all four operations:

- \`Add\`
- \`Subtract\`
- \`Multiply\`
- \`Divide\`

For division by zero, return \`Error::DivisionByZero\`. For an invalid enum value, return \`Error::InvalidOperation\`. Successful operations return \`Error::None\` and the numeric result.

## Public interface

The platform contract is:

\`\`\`cpp
namespace cppapplab::calculator {

enum class Operation { Add, Subtract, Multiply, Divide };
enum class Error { None, DivisionByZero, InvalidOperation };

struct Result {
    double value{};
    Error error{Error::None};
};

Result calculate(Operation operation, double left, double right) noexcept;

}
\`\`\`

The header in your learner workspace is provided for reference and editor assistance. Platform acceptance compiles your implementation against a platform-owned copy of this interface, so changing the learner header cannot weaken or redefine the contract.

## Build it yourself

From the learner workspace:

\`\`\`bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include \\
  src/calculator.cpp src/main.cpp -o build/calculator

./build/calculator add 2 3
./build/calculator divide 7.5 2.5
./build/calculator divide 1 0
\`\`\`

Expected command-line behavior:

- \`add 2 3\` prints \`5\` and exits with code 0.
- \`divide 7.5 2.5\` prints \`3\` and exits with code 0.
- \`divide 1 0\` prints \`error: division by zero\` to standard error and exits with code 3.

## What the platform checks

The explicit \`cppapplab check\` command:

1. fingerprints the declared learner source and interface files
2. stages only the declared C++ implementation files
3. configures a platform-owned CMake fixture
4. compiles the learner implementation against the fixed interface
5. runs seven platform-owned GoogleTest cases
6. runs two CLI smoke cases, including division by zero
7. records the result, source fingerprint, and bounded log

The learner's own build files and tests are not used as the acceptance authority.

## Explain your design

1. Why is the function declaration in a header while its implementation is in a separate \`.cpp\` file?
2. Why should division by zero be represented as an error rather than returning an arbitrary numeric value?
3. Why does the platform compile against its own copy of the interface header?

## Acceptance

- [ ] The implementation compiles as C++17 with warnings treated as errors.
- [ ] Addition, subtraction, multiplication, and division return correct values.
- [ ] Division by zero returns \`Error::DivisionByZero\`.
- [ ] Invalid operation values return \`Error::InvalidOperation\`.
- [ ] All seven platform-owned unit tests pass.
- [ ] Both command-line smoke cases pass.
- [ ] The learner source has not changed during the check.
