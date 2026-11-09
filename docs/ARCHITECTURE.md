# CppAppLab Architecture

## 1. Product definition

CppAppLab is a local-first C++ learning and engineering-practice platform. It combines course material, isolated learner workspaces, reference projects, fixed acceptance tests, and reproducible evidence.

It is not intended to be a video-course index or a directory of completed example programs.

## 2. Main subsystems

### Course catalog

Owns the curriculum map, lesson material, task cards, prerequisites, expected artifacts, and platform/toolchain requirements. Course material explains the engineering concepts; task cards define what the learner must implement.

### Learner workspace

Contains the learner's editable source tree, build configuration, and personal work. It must not share mutable source files with the reference implementation or platform acceptance fixtures.

### Reference projects

Contain complete, runnable solutions used for teaching, regression, and comparison. They are not copied into a learner workspace after a task starts and are never counted as learner work.

### Acceptance engine

Builds the learner project and executes platform-owned checks. Learner-provided tests may be run as supplemental tests, but cannot replace or weaken the fixed acceptance contract. A check records compiler output, test results, toolchain metadata, source fingerprint, and exit status.

### Learning navigator

Provides the planned P1 interaction model: start a new project, read a lesson, request progressive hints, check the current implementation, inspect the first failure, and continue to the next task when prerequisites are satisfied.

## 3. Intended learning loop

\`\`\`text
course / task card
       |
       v
learner workspace
       |
       v
edit implementation
       |
       v
platform-controlled configure + build
       |
       v
platform-owned acceptance tests
       |
       +---- failure --> diagnostic + hint --> revise
       |
       v
source fingerprint + evidence record
       |
       v
next eligible task
\`\`\`

A previous pass becomes historical evidence when the source fingerprint changes. It must not be presented as proof that the current source still passes.

## 4. Repository boundaries

- \`course/\`: curriculum and instructional material.
- \`labs/\`: focused concept exercises.
- \`projects/\`: cumulative, independently runnable work products.
- \`include/\`, \`src/\`: platform implementation.
- \`tests/\`: platform-owned regression and acceptance tests.
- \`scripts/\`: environment checks and maintainer workflows.
- \`docs/\`: architecture, decisions, status, and contributor guidance.

## 5. Execution and safety boundary

The first version runs locally in the learner's WSL2 Ubuntu environment. It is a learning tool, not a hostile-code sandbox. Learners must use disposable workspaces and must not run untrusted code or tests against sensitive files, credentials, or production services.

The platform should use bounded execution time and output limits when the acceptance engine is implemented. Those controls improve reliability but must not be described as operating-system-level isolation.

## 6. First-version technical baseline

- C++17
- GCC on Ubuntu 24.04 LTS under WSL2
- CMake 3.18 or newer
- GDB
- GoogleTest / CTest
- clang-format / clang-tidy
- AddressSanitizer / UndefinedBehaviorSanitizer
- GitHub Actions on Ubuntu 24.04

POSIX and Linux-specific labs use real system interfaces. Portability is introduced only where it preserves the behavior being taught.
