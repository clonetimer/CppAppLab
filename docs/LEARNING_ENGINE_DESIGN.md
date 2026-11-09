# Learning Engine — P1 Design

## Current vertical slice

The first implemented learning loop is W1 Hello World:

\`\`\`text
list
  -> start
  -> lesson
  -> hint 1 / 2 / 3
  -> edit learner source
  -> check
  -> status
  -> next
\`\`\`

Only explicit \`check\` compiles and executes learner code. \`list\`, \`lesson\`, \`hint\`, \`status\`, and \`next\` are navigation/read-only operations.

## Workspace boundary

- A task starts by copying only its learner template.
- Reference source remains in the repository's reference tree and is not copied to the learner workspace.
- Existing destinations are never overwritten.
- The workspace marker identifies the task and platform version.
- Build output and local check records are stored under \`.cppapplab/\` inside the learner workspace.

## Check contract for W1 Hello World

The current task uses a fixed platform-owned contract:
- source file: \`src/main.cpp\`
- compiler: GCC with \`-std=c++17 -Wall -Wextra -Wpedantic -Werror\`
- maximum compile time: 30 seconds
- maximum program time: 5 seconds
- bounded combined output capture
- expected output: exactly \`Hello, CppAppLab!\\n\`

The learner's CMakeLists.txt and tests are not consulted.

## Evidence semantics

The local evidence and log are navigation records, not certificates. They are user-editable files and must never be represented as tamper-proof proof of learning.

The source fingerprint uses FNV-1a 64-bit to detect ordinary source changes. It is not a cryptographic integrity mechanism. A changed fingerprint marks the recorded result stale; learners must rerun the explicit check.

A passing check proves only that the declared source, at that moment, compiled and produced the required output under the recorded constraints. It does not prove conceptual understanding or provide a security boundary.

## Execution boundary

The process runner uses direct \`execvp\` argument vectors (no shell interpolation), captures stdout/stderr, enforces a time limit, and kills the process group on timeout. This is operational control, not a hostile-code sandbox. Learner programs still run with the learner's operating-system permissions.

## Next vertical slice

W1 Calculator will add a multi-file learner project and a platform-owned CMake/GoogleTest fixture. The fixture will compile the learner implementation against a platform-owned public interface and acceptance tests; it will not trust the learner's own CMakeLists.txt or tests.
