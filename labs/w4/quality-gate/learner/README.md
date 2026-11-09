# W4 · TaskManager Quality Gate

This workspace is the W4 quality-engineering exercise built on the W3 TaskManager.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The starter learner test suite intentionally fails. Replace each TODO with meaningful GoogleTest assertions before expecting CTest to pass.

## Sanitizers

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
```

## clang-format and clang-tidy

```bash
find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror

cmake -S . -B build-tidy \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_CLANG_TIDY=ON
cmake --build build-tidy --parallel 2
```

## GDB

Run one test in isolation:

```bash
gdb --args ./build/task_manager_tests \
  --gtest_filter=TaskManagerLearnerTest.CopyOperationsAreDeep
```

Useful commands include `break`, `run`, `next`, `step`, `print`, `bt`, and `continue`. Debug the failing assertion rather than changing expected values to silence it.

## Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 ./build/task_manager_tests
```

Valgrind is an additional Linux check; ASan/UBSan remain separate build configurations.

## One-command quality gate

```bash
./scripts/quality-check.sh
```

## Platform acceptance

```bash
./build/cppapplab check "/absolute/path/to/your/quality-gate-workspace"
```

The platform check uses its own fixed CMake/GoogleTest fixture. It does not execute this workspace's CMakeLists.txt as the acceptance authority.
