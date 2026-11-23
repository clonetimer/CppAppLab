# W4 · TaskManager 质量门禁

## 从这里开始

本课从一个可以运行的应用和十个刻意留待完成的学员测试开始。你的首要任务是理解一个失败的测试，不必立刻让整个质量检查流程全部通过。

1. 以 Debug 模式构建项目。
2. 运行 CTest，阅读第一个失败测试的名称。
3. 打开该测试，找出准备、执行和断言三个部分。
4. 将一个 TODO 替换为检查可观察行为的断言。
5. 使用示例中的 GoogleTest 过滤器，只运行该测试；然后运行完整测试套件。
6. 常规测试通过后，再研究独立的 Sanitizer、GDB、格式化和静态分析任务。

**各工具回答的问题：**GoogleTest 检查预期行为；GDB 帮助观察执行过程；ASan/UBSan 查找特定类型的内存缺陷和未定义行为；clang-tidy 报告静态代码质量问题。它们相互补充，不能互相替代。

应用演示运行正常，并不等于通过质量门禁：学员编写的测试和可复现的构建配置也是本任务的一部分。

## 目标

将 W3 的 TaskManager 重构成果变成可复现、由测试驱动的工程交付物。本周围绕行为验证、故障诊断和项目的可维护性展开，不再增加新的应用功能。

初始工作区包含已完成的 W3 实现，以及由学员维护的 GoogleTest 套件，其中十个测试被刻意设置为失败。你的任务是用有意义的断言替换这些 TODO 失败，再运行完整的质量检查流程。

## 你的交付物

1. `tests/task_manager_test.cpp` 中包含十个测试的学员测试套件。
2. 能够正常工作的 CMake 构建配置，将应用和测试分为独立目标。
3. 可重复执行的 CTest 流程。
4. 相互独立的 ASan/UBSan 和 clang-tidy 构建配置。
5. 通过 clang-format 检查的 C++ 源代码。
6. 记录全新构建、测试、调试和质量检查命令的 README。

## 测试驱动流程

从第一个失败测试开始：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

初始套件会有意报告十个 TODO 失败。每次用断言替换一个 TODO。使用 GoogleTest 过滤器缩短反馈周期：

```bash
./build/task_manager_tests \
  --gtest_filter=TaskManagerLearnerTest.ManagerAddsAndFindsTasks
```

采用“准备—执行—断言”（Arrange–Act–Assert）模式：

- 准备：创建管理器和所需的最少记录。
- 执行：调用一个公共操作。
- 断言：验证返回结果、可观察状态，以及相关场景下应该保持不变的状态。

十个学员测试用例覆盖：

| 测试 | 必须验证的内容 |
|---|---|
| TaskConstructionRejectsInvalidState | 构造函数的不变量 |
| TaskEqualityAndOrderingUseIds | 身份与排序 |
| ManagerAddsAndFindsTasks | 添加与查找 |
| ManagerRejectsInvalidAndDuplicateIds | 输入验证 |
| ManagerMarksAndRemovesTasks | 状态变更与删除 |
| NextIdHandlesGapsAndOverflow | 边界条件 |
| CopyOperationsAreDeep | 拷贝后所有权相互独立 |
| MoveOperationsTransferOwnership | 目标与移动后的源对象状态 |
| SnapshotReturnsOrderedCopies | 排序与值副本的独立性 |
| PersistencePreservesW2FormatAndRejectsMalformedData | W2 兼容性与事务式加载 |

## CMake 与 CTest

提供的 CMake 初始配置会构建：

- `todo_core`：可复用的 Task / TaskManager 库
- `todo`：命令行应用
- `task_manager_tests`：由你维护的 GoogleTest 可执行文件

CTest 会分别发现每个 GoogleTest 用例。保持测试相互独立：使用临时目录，避免固定的用户数据路径，不依赖测试执行顺序。

## 使用 GDB 调试

在 GDB 中运行失败的测试可执行文件：

```bash
gdb --args ./build/task_manager_tests \
  --gtest_filter=TaskManagerLearnerTest.CopyOperationsAreDeep
```

常用命令：

- `break` — 设置断点
- `run` — 启动选中的测试
- `next` / `step` — 跟踪执行过程
- `print variable` — 检查状态
- `bt` — 查看调用栈
- `continue` — 继续执行

一份有用的调试报告应明确指出失败的断言、观察到的状态、预期的不变量和根本原因。

## Sanitizer 与内存诊断

使用独立构建目录，避免 Sanitizer 选项影响普通 Debug 构建：

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
```

如果安装了 Valgrind，运行：

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 ./build/task_manager_tests
```

ASan/UBSan 和 Valgrind 是互补的诊断工具。一次无报错运行只能为已执行的路径提供证据，不能证明所有可能的输入都是安全的。

## 格式化与静态分析

在不修改文件的情况下检查格式：

```bash
find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
```

启用 clang-tidy 后构建：

```bash
cmake -S . -B build-tidy \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPPAPPLAB_ENABLE_CLANG_TIDY=ON
cmake --build build-tidy --parallel 2
```

对于指出真实问题的警告，应予以修复。如果有意保留某条警告，需要记录原因；不要只为让构建通过，就屏蔽整类警告。

## 可复现的质量门禁

按顺序运行上面的命令：Debug 构建与 CTest、独立的 ASan/UBSan 构建与测试、格式检查，然后是启用 clang-tidy 的编译。如果安装了 Valgrind，再加入可选的 Valgrind 检查。

在 README 中记录准确的命令和工具版本。全新构建必须能在新建的构建目录中成功，不能只依赖开发者已有的缓存。

## 平台验收

运行：

```bash
./build/cppapplab check "/absolute/path/to/your/quality-gate-workspace"
```

平台固定的验收环境会运行 30 个平台维护的行为测试、你编写的十个学员测试，以及一个多进程 CLI 工作流测试：共 41 个测试。

平台使用自己维护的固定 CMake 文件和公共头文件。它会编译你的学员测试源代码，但不会执行学员的 CMakeLists.txt 作为验收依据。

## 故障排查

- **应用可以运行，但十个学员测试都因 TODO 信息而失败。**打开 `tests/task_manager_test.cpp`，检查报告中的 `FAIL()` 所在行。这些是预设的测试编写练习；用针对测试名称所描述行为的断言替换一个 TODO，然后重新构建并运行该用例。
- **过滤后执行了零个测试。**将过滤条件与测试源代码中的套件名和用例名精确比较。重新构建 `build/task_manager_tests`，再次运行上面的 `TaskManagerLearnerTest.ManagerAddsAndFindsTasks` 示例，并在运行 CTest 之前确认它确实执行了一个测试。
- **持久化测试单独运行通过，但在套件中运行失败。**检查它的文件路径以及对现有记录的假设。使用提供的 `TemporaryDirectory` 辅助类，并在该测试内部创建输入，让结果不依赖其他测试的文件或执行顺序。
- **正式验收通过，但某个本地质量检查步骤失败。**检查失败的是哪个阶段：固定验收环境不会运行学员的 CMake 配置。在对应的独立构建目录（`build`、`build-sanitize` 或 `build-tidy`）中复现该阶段，处理诊断信息后再重新运行完整流程。

## 验收要求

- [ ] 十个学员测试都包含有意义的断言，并全部通过。
- [ ] W2 持久化格式和 CLI 行为保持不变。
- [ ] CMake 能从全新目录构建库、应用和测试可执行文件。
- [ ] CTest 报告全部 41 个平台与学员检查通过。
- [ ] ASan/UBSan 构建和测试在独立配置下通过。
- [ ] clang-format 未报告格式违规。
- [ ] 启用 clang-tidy 的构建完成，且已审阅警告。
- [ ] 使用 GDB 诊断至少一个刻意设置为失败的测试。
- [ ] 在 Valgrind 可用时运行它，并记录结果。
- [ ] README 记录可复现的命令和已知限制。
