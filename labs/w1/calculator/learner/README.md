# W1 · Multi-file Calculator

This is an independent learner workspace. Implement \`src/calculator.cpp\` using the public contract in \`include/calculator.hpp\`.

Build locally:

\`\`\`bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include \\
  src/calculator.cpp src/main.cpp -o build/calculator
\`\`\`

Run examples:

\`\`\`bash
./build/calculator add 2 3
./build/calculator divide 1 0
\`\`\`

Then run the platform-owned check from the CppAppLab repository:

\`\`\`bash
./build/cppapplab check "/absolute/path/to/your/calculator-workspace"
\`\`\`

The platform compiles against a fixed interface and its own GoogleTest suite. The local evidence file is navigation data, not a certificate.
