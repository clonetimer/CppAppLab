# W1 · First C++ Program

## Goal

Compile and run a small C++17 program with GCC. This is the first complete edit → build → execute → check loop in CppAppLab.

## Your task

Edit only:

\`\`\`text
src/main.cpp
\`\`\`

Make the program print exactly:

\`\`\`text
Hello, CppAppLab!
\`\`\`

The output must end with one newline. The program must then return exit code 0.

## What the platform checks

When you run \`cppapplab check\`, the platform:

1. Computes a fingerprint of the declared learner source file.
2. Invokes GCC with C++17 and warning flags.
3. Runs the compiled executable with a short time limit.
4. Compares the captured output byte-for-byte with the expected output.
5. Records the result, source fingerprint, and bounded log.

The platform does not execute your own CMakeLists.txt or learner-provided tests for this first task.

## Build it yourself

From the learner workspace:

\`\`\`bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -g src/main.cpp -o build/hello
./build/hello
\`\`\`

Then return to the CppAppLab repository and run the fixed check:

\`\`\`bash
./build/cppapplab check "/absolute/path/to/your/hello-workspace"
\`\`\`

## Understand the program

- \`#include <iostream>\` makes the standard output stream available.
- \`main\` is the program entry point.
- \`std::cout\` writes text to standard output.
- \`\\n\` ends the output line.
- \`return 0\` reports successful process termination.

## Explain your result

After the check passes, explain in your own words:

1. What is the difference between compiling and running?
2. Why does the platform compare exact output instead of only checking the process exit code?
3. What changes when you remove the newline?

## Acceptance

- [ ] The source compiles as C++17.
- [ ] The executable exits with code 0.
- [ ] Standard output is exactly \`Hello, CppAppLab!\\n\`.
- [ ] The learner source has not changed during the check.
