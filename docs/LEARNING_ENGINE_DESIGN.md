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
- The destination directory is reserved before copying; an existing destination is never overwritten.
- Template symlinks and unsupported file types are rejected.
- The workspace marker identifies the task and platform version.
- Build output and local check records are stored under \`.cppapplab/\` inside the learner workspace.

## Check contract for W1 Hello World

The current task uses a fixed platform-owned contract:

- source file: \`src/main.cpp\`
- compiler: GCC with \`-std=c++17 -Wall -Wextra -Wpedantic -Werror\`
- maximum compile time: 30 seconds
- maximum program time: 5 seconds
- combined stdout/stderr capture is bounded
- stdin is redirected from \`/dev/null\`
- timeout kills the process group, not only the direct child
- expected stdout: exactly \`Hello, CppAppLab!\\n\`
- process exit code: 0

The learner's CMakeLists.txt and tests are not consulted. The acceptance contract is owned by the platform task registry.

## Evidence semantics

The local evidence and log are navigation records, not certificates. They are user-editable files and must never be represented as tamper-proof proof of learning.

The source fingerprint uses FNV-1a 64-bit to detect ordinary source changes. It is not a cryptographic integrity mechanism. Declared source paths are checked for path traversal and symlink components. A changed fingerprint marks the recorded result stale; learners must rerun the explicit check.

A passing check proves only that the declared source, at that moment, compiled and produced the required output under the recorded constraints. It does not prove conceptual understanding or provide a security boundary.

## Execution boundary

The process runner uses direct \`execvp\` argument vectors (no shell interpolation), captures stdout/stderr, redirects stdin from \`/dev/null\`, enforces a time limit, bounds captured output, and kills the process group on timeout. This is operational control, not a hostile-code sandbox. Learner programs still run with the learner's operating-system permissions.

## Automated coverage

The current hosted Ubuntu 24.04 CI run passed 15/15 tests in both the ordinary GCC build and the ASan/UBSan build. Coverage includes task lookup, output capture, stdin behavior, timeout and process-group termination, output limits, workspace creation/no-overwrite, source fingerprint changes, symlink rejection, and the end-to-end check/stale-evidence loop.

## Next vertical slice

W1 Calculator will add a multi-file learner project and a platform-owned CMake/GoogleTest fixture. The fixture will compile the learner implementation against a platform-owned public interface and acceptance tests; it will not trust the learner's own CMakeLists.txt or tests.
