# W4 · TaskManager Quality Gate

## Goal

Turn the W3 TaskManager refactor into a reproducible, test-driven engineering deliverable. This week is about proving behavior, diagnosing failures, and keeping the project maintainable—not adding another application feature.

The starter workspace contains the completed W3 implementation and a learner-owned GoogleTest suite with ten intentionally failing tests. Your job is to replace the TODO failures with meaningful assertions, then run the complete quality workflow.

## Your deliverables

1. A ten-test learner suite in `tests/task_manager_test.cpp`.
2. A working CMake build with separate application and test targets.
3. A repeatable CTest workflow.
4. Separate ASan/UBSan and clang-tidy build configurations.
5. clang-format-clean C++ sources.
6. A README that records clean build, test, debugging, and quality commands.

## Test-driven workflow

Start with the first failing test:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The starter suite intentionally reports ten TODO failures. Replace one TODO at a time with assertions. Use the GoogleTest filter to shorten the feedback loop:

```bash
./build/task_manager_tests \
  --gtest_filter=TaskManagerLearnerTest.ManagerAddsAndFindsTasks
```

Use the Arrange–Act–Assert pattern:

- Arrange: create a manager and the minimum records needed.
- Act: invoke one public operation.
- Assert: verify the result, observable state, and unchanged state where relevant.

The ten learner test cases cover:

| Test | Required evidence |
|---|---|
| TaskConstructionRejectsInvalidState | Constructor invariants |
| TaskEqualityAndOrderingUseIds | Identity and ordering |
| ManagerAddsAndFindsTasks | Add and lookup |
| ManagerRejectsInvalidAndDuplicateIds | Input validation |
| ManagerMarksAndRemovesTasks | State transition and removal |
| NextIdHandlesGapsAndOverflow | Boundary conditions |
| CopyOperationsAreDeep | Independent ownership after copying |
| MoveOperationsTransferOwnership | Destination and moved-from states |
| SnapshotReturnsOrderedCopies | Ordering and value-copy independence |
| PersistencePreservesW2FormatAndRejectsMalformedData | W2 compatibility and transactional load |

## CMake and CTest

The supplied CMake starter builds:

- `todo_core`: the reusable Task / TaskManager library
- `todo`: the command-line application
- `task_manager_tests`: your learner-owned GoogleTest executable

CTest discovers each GoogleTest case separately. Keep tests independent: use temporary directories, avoid fixed user data paths, and do not depend on test execution order.

## Debugging with GDB

Run the failing test executable under GDB:

```bash
gdb --args ./build/task_manager_tests \
  --gtest_filter=TaskManagerLearnerTest.CopyOperationsAreDeep
```

Useful commands:

- `break` — set a breakpoint
- `run` — start the selected test
- `next` / `step` — follow execution
- `print variable` — inspect state
- `bt` — inspect the call stack
- `continue` — resume execution

A useful debugging report identifies the failing assertion, the observed state, the expected invariant, and the root cause.

## Sanitizers and memory diagnostics

Use a separate build directory so sanitizer flags do not contaminate the normal Debug build:

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
```

If Valgrind is installed, run:

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 ./build/task_manager_tests
```

ASan/UBSan and Valgrind are complementary diagnostics. A clean run is evidence for the executed paths, not a proof that every possible input is safe.

## Formatting and static analysis

Check formatting without modifying files:

```bash
find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
```

Build with clang-tidy enabled:

```bash
cmake -S . -B build-tidy \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_CLANG_TIDY=ON
cmake --build build-tidy --parallel 2
```

Resolve warnings where they identify real issues. If a warning is intentionally retained, document why; do not suppress a whole category just to obtain a green build.

## Reproducible quality gate

Run the supplied script:

```bash
./scripts/quality-check.sh
```

It performs a clean Debug build, CTest, a separate ASan/UBSan build, formatting checks, clang-tidy-enabled compilation, and an optional Valgrind pass.

Record the exact commands and tool versions in the README. A clean build must work from a new build directory, not only from a developer's existing cache.

## Platform acceptance

Run:

```bash
./build/cppapplab check "/absolute/path/to/your/quality-gate-workspace"
```

The fixed platform fixture runs 30 platform-owned behavior tests, your ten learner-authored tests, and one multi-process CLI workflow test: 41 tests total.

The platform uses its own fixed CMake file and public headers. It compiles your learner test source but does not execute your learner CMakeLists.txt as the acceptance authority.

## Acceptance

- [ ] All ten learner tests contain meaningful assertions and pass.
- [ ] The W2 persistence format and CLI behavior remain unchanged.
- [ ] CMake builds the library, application, and test executable from a clean directory.
- [ ] CTest reports all 41 platform and learner checks passing.
- [ ] ASan/UBSan build and test pass independently.
- [ ] clang-format reports no violations.
- [ ] clang-tidy-enabled build completes; warnings are reviewed.
- [ ] GDB is used to diagnose at least one deliberately failing test.
- [ ] Valgrind is run when available, with the result recorded.
- [ ] README documents reproducible commands and known limitations.
