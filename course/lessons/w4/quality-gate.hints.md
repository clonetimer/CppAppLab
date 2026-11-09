## Hint 1

Do not replace the ten failing tests with empty bodies. Each test should prove one observable contract. Start with a minimal Arrange–Act–Assert structure and assert both the returned result and the resulting state.

## Hint 2

Use the public API rather than reaching into private members:

- `Task::id()`, `title()`, and `completed()` for value observations
- `TaskManager::find()` for a non-owning lookup
- `snapshot()` for an ordered value copy
- `size()` and `empty()` for collection state

For copy and move tests, compare object addresses as well as values. Equal values do not prove independent ownership.

## Hint 3

Keep diagnostics separate:

1. Run one GoogleTest case with `--gtest_filter`.
2. Use GDB to inspect the failing state and call stack.
3. Run the normal Debug suite through CTest.
4. Build a separate ASan/UBSan directory.
5. Run clang-format in check-only mode.
6. Enable clang-tidy in another build directory.
7. Run Valgrind when installed and record the exact command.

Do not change expected values merely to silence a failing test. Explain the invariant that the test is intended to protect.
