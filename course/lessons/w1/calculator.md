# W1 · Multi-file Calculator

## Start here

You are not expected to understand every C++ header immediately. First follow one value through the program.

1. Open `src/main.cpp` and find where it calls `calculate`.
2. Open `include/calculator.hpp` to see the function's input and output types.
3. Implement the function in `src/calculator.cpp`; do not rewrite the provided command-line front end.
4. Test one easy case first: `add 2 3`. Then test subtraction, multiplication and division.
5. Test the important error case: `divide 1 0`. A correct program must report the error rather than crash or return an ordinary number.

**Useful mental model:** the header describes what callers can ask for; the source file contains how the answer is calculated. The compiler links both files into one program.

If the compiler reports an undefined reference, check whether the function definition matches the namespace, name, parameters and return type in the header.

## Goal

Extend the first C++ program into a multi-file application. Separate a public interface from its implementation, compile multiple translation units, and handle an arithmetic error explicitly.

## Your task

Implement the function declared in:

```text
include/calculator.hpp
```

in:

```text
src/calculator.cpp
```

The command-line front end in `src/main.cpp` is provided. It parses the operation and operands, calls your `calculate` function, and prints the result or a diagnostic.

Implement all four operations:

- `Add`
- `Subtract`
- `Multiply`
- `Divide`

For division by zero, return `Error::DivisionByZero`. For an invalid enum value, return `Error::InvalidOperation`. Successful operations return `Error::None` and the numeric result.

## Public interface

The platform contract is:

```cpp
namespace cppapplab::calculator {

enum class Operation { Add, Subtract, Multiply, Divide };
enum class Error { None, DivisionByZero, InvalidOperation };

struct Result {
    double value{};
    Error error{Error::None};
};

Result calculate(Operation operation, double left, double right) noexcept;

}
```

The header in your learner workspace is provided for reference and editor assistance. Platform acceptance compiles your implementation against a platform-owned copy of this interface, so changing the learner header cannot weaken or redefine the contract.

## Build it yourself

From the learner workspace:

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include \
  src/calculator.cpp src/main.cpp -o build/calculator

./build/calculator add 2 3
./build/calculator divide 7.5 2.5
./build/calculator divide 1 0
```

Expected command-line behavior:

- `add 2 3` prints `5` and exits with code 0.
- `divide 7.5 2.5` prints `3` and exits with code 0.
- `divide 1 0` prints `error: division by zero` to standard error and exits with code 3.

## What the platform checks

The explicit `cppapplab check` command:

1. fingerprints the declared learner source and interface files
2. stages only the declared C++ implementation files
3. configures a platform-owned CMake fixture
4. compiles the learner implementation against the fixed interface
5. runs seven platform-owned GoogleTest cases
6. runs two CLI smoke cases, including division by zero
7. records the result, source fingerprint, and bounded log

The learner's own build files and tests are not used as the acceptance authority.

## Explain your design

1. Why is the function declaration in a header while its implementation is in a separate `.cpp` file?
2. Why should division by zero be represented as an error rather than returning an arbitrary numeric value?
3. Why does the platform compile against its own copy of the interface header?

## Troubleshooting

- **The linker reports an undefined reference to `calculate`.** Compare its definition with `include/calculator.hpp`, including the namespace and parameter types. Use the build command above with both `src/calculator.cpp` and `src/main.cpp`, then rebuild.
- **Every operation prints `0`.** Inspect the starter's `return {}` in `src/calculator.cpp`: it supplies a default result without doing arithmetic. Implement one operation first and check both `Result::value` and `Result::error` before adding the others.
- **`divide 1 0` exits with code 3.** Read its diagnostic: `error: division by zero` is the expected CLI behavior. If it prints an ordinary number instead, inspect the division branch and return the declared error through `Result`.
- **The four CLI commands work but the invalid-operation test fails.** The CLI rejects unknown words before calling `calculate`, while the fixed test passes `static_cast<Operation>(99)` directly. Inspect the function's handling of values outside the four named operations and return `Error::InvalidOperation` for them.

## Acceptance

- [ ] The implementation compiles as C++17 with warnings treated as errors.
- [ ] Addition, subtraction, multiplication, and division return correct values.
- [ ] Division by zero returns `Error::DivisionByZero`.
- [ ] Invalid operation values return `Error::InvalidOperation`.
- [ ] All seven platform-owned unit tests pass.
- [ ] Both command-line smoke cases pass.
- [ ] The learner source has not changed during the check.

## Independent language fundamentals

Optional, independent C++17 knowledge experiments. Open **查知识** in the workbench, search the identifier or term, predict the result, run the fixed failure and repair, then explain and try a separate transfer. These probes do not change the original project or formal acceptance. Full fixed sources and actual outputs are in the knowledge panel.

- **scope** — A block-local name is not visible outside its declaring block. Place the result in the narrowest enclosing scope that needs it. Predict whether value can be accessed after the block ends.
- **narrowing** — List initialization rejects implicit narrowing such as double to int. Choose the type first; convert explicitly only when appropriate. Predict whether int value{42.5} compiles.
- **value-reference** — Value parameters are copies; mutable reference parameters alias the caller's object. The negative program safely observes the unchanged original. Predict the original value after bump(int value).
- **scoped-enum** — Scoped enum values do not implicitly become integers. Use typed comparisons; explicitly convert only at a numeric boundary. Predict whether an enum class value initializes int directly.
