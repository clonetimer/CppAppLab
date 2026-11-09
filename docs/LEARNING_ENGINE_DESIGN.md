# Learning Engine — P1 Design

## Current learning loop

The available task sequence begins with W1 Hello World and continues to the W1 multi-file Calculator:

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
- The destination directory is reserved before copying; an existing destination is never overwritten.
- Template symlinks and unsupported file types are rejected.
- The workspace marker identifies the task and platform version.
- Build output and local check records are stored under \`.cppapplab/\` inside the learner workspace.

## W1 Hello World check contract

- source file: \`src/main.cpp\`
- compiler: GCC with \`-std=c++17 -Wall -Wextra -Wpedantic -Werror\`
- maximum compile time: 30 seconds
- maximum program time: 5 seconds
- combined stdout/stderr capture is bounded
- stdin is redirected from \`/dev/null\`
- timeout kills the process group
- expected stdout: exactly \`Hello, CppAppLab!\\n\`
- process exit code: 0

## W1 Calculator check contract

The calculator task is built through a platform-owned CMake fixture, not the learner's CMakeLists.txt.

- learner source files are fingerprinted before and after checking
- only declared \`.cpp\` implementation files are staged into a controlled build directory
- the fixture supplies its own fixed \`calculator.hpp\` interface
- the fixture builds the learner library and CLI with C++17 and warnings-as-errors
- seven platform-owned GoogleTest cases cover four operations, division by zero, and negative/zero operands
- two CLI smoke checks cover a normal calculation and division by zero
- the learner's CMakeLists.txt, tests, and copied header cannot replace the platform acceptance contract
- configure, build, CTest, and CLI execution all have bounded time/output

The calculator task records 9 acceptance checks: seven fixed unit tests and two CLI smoke checks.

## Evidence semantics

The local evidence and log are navigation records, not certificates. They are user-editable files and must never be represented as tamper-proof proof of learning.

The source fingerprint uses FNV-1a 64-bit to detect ordinary source changes. It is not a cryptographic integrity mechanism. Declared source paths are checked for path traversal and symlink components. A changed fingerprint marks the recorded result stale; learners must rerun the explicit check.

A passing check proves only that the declared source, at that moment, compiled and passed the fixed behavior contract. It does not prove conceptual understanding or provide a security boundary.

## Execution boundary

The process runner uses direct \`execvp\` argument vectors (no shell interpolation), captures stdout/stderr, redirects stdin from \`/dev/null\`, enforces a time limit, bounds captured output, and kills the process group on timeout. This is operational control, not a hostile-code sandbox. Learner programs still run with the learner's operating-system permissions.

## Next vertical slice

P1-C will add cross-workspace prerequisite/progress state and task-catalog externalization, followed by more task packs. The next course work product after Calculator is W2 Todo CLI.
