# W3 · Task / TaskManager 重构

## 从这里开始

W3 改变的是内部设计，学员使用的 Todo 命令保持不变。可以将它理解为从记录列表转向具有清晰所有权的对象。

1. 先阅读 `Task` 的公共操作。一个 `Task` 表示一个 Todo 条目。
2. 阅读 `TaskManager` 的操作。它持有并协调多个 `Task` 对象。
3. 先实现构造和简单的访问器，再处理拷贝与移动操作。
4. 实现添加、查找、标记完成和删除。确认 CLI 行为与 W2 一致。
5. 添加深拷贝和移动语义，然后测试拷贝出的管理器是否拥有独立的对象。
6. 最后完成持久化兼容性，确保 W2 数据文件仍能正确加载。

**通俗理解所有权：**管理器负责其任务对象的生命周期。`unique_ptr` 表示一个明确的所有者；`find` 返回的裸指针只是临时查看对象的方式，不代表拥有对象，也不能据此删除对象。

不要一次实现所有进阶规则。先让简单的对象生命周期正确，再添加拷贝、移动和异常安全保证。

## 目标

将 W2 Todo CLI 的核心重构为面向对象的设计，保持命令行行为和磁盘数据格式不变。本任务重点学习类不变量、封装、RAII、智能指针所有权、拷贝与移动语义，以及运算符重载。

你将构建两个相互协作的类：

- `Task`：拥有自己的标题，并通过受控接口对外提供操作的值对象
- `TaskManager`：Task 对象的所有者和协调者

## 你的任务

实现以下文件中的声明：

- `include/task.hpp`
- `include/task_manager.hpp`

将实现写在：

- `src/task.cpp`
- `src/task_manager.cpp`

`src/main.cpp` 中的命令行前端已提供。保留 W2 的命令：

- `todo add <title>`
- `todo list`
- `todo done <id>`
- `todo remove <id>`

文件格式仍为 `CPPAPPLAB_TODO_V1`，因此 W3 仍能读取 W2 生成的数据。

## 设计约定

### Task

`Task` 拥有自己的 ID、标题和完成状态。

- 构造时拒绝零 ID，以及空标题或仅含空白字符的标题。
- 通过访问器读取状态；通过 `mark_done()` 修改完成状态。
- 拷贝与移动操作使用其拥有的 `std::string` 的值语义。
- `operator==` 根据 ID 比较任务身份。
- `operator<` 根据 ID 对任务排序。
- `operator<<` 将任务格式化为 `#id [x] title`。

### TaskManager

`TaskManager` 使用以下容器拥有任务：

```cpp
std::vector<std::unique_ptr<Task>> tasks_;
```

这个拥有对象的容器绝不能向调用者暴露所有权。`find()` 返回非拥有指针，而 `snapshot()` 返回按 ID 排序的值副本。

需要实现：

- 添加、查找、标记完成、删除、获取下一个 ID、获取大小、判断为空以及生成快照的操作。
- 深拷贝构造函数，创建彼此独立的 `Task` 对象。
- 拷贝赋值，保证自赋值安全，并采用拷贝后交换（copy-and-swap）等提供强异常安全保证的策略。
- 移动构造和移动赋值，转移所有权，并让源管理器保持为空且有效。
- `swap()`，交换所拥有的集合。

删除任务时，必须通过 `std::unique_ptr` 自动释放其对象。不要使用拥有对象的裸指针，也不要手动使用 `new` / `delete`。

## 持久化兼容性

保留 W2 的数据格式和存储行为：

- 版本头：`CPPAPPLAB_TODO_V1`
- 记录字段：ID、完成标志、经过转义的标题
- 转义字符：反斜杠、制表符、换行符、回车符
- 文件大小上限：1 MiB
- 标题大小上限：4,096 字节
- 拒绝格式错误的记录、重复 ID 和符号链接路径
- 加载到临时 `TaskManager` 中，只有完全验证后才替换当前状态
- 通过同目录下的临时文件保存，再进行重命名

默认文件仍为 `~/.local/share/cppapplab/todos.tsv`；`CPPAPPLAB_TODO_FILE` 用于选择其他文件。

## 本地构建

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/task.cpp src/task_manager.cpp src/main.cpp -o build/todo

export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review unique ownership"
./build/todo add "Verify copy and move"
./build/todo list
./build/todo done 1
./build/todo remove 2
```

## 平台检查什么

运行：

```bash
./build/cppapplab check "/absolute/path/to/your/task-manager-workspace"
```

固定验收环境运行 30 个 GoogleTest 用例和一个 CLI 工作流测试。它会检查值不变量、身份与排序运算符、深拷贝、拷贝赋值、自赋值、移动构造、移动赋值、快照排序、W2 文件兼容性、格式错误的数据，以及事务式加载。CLI 测试验证重构后的类在多次独立进程调用之间仍保留 W2 的命令行为。

平台使用自己维护的两个头文件副本来编译你的实现。学员可修改的 CMake 文件和测试不作为验收依据。

## 解释你的设计

1. 为什么 `TaskManager` 持有 `std::unique_ptr<Task>`，而不是暴露拥有对象的裸指针？
2. 为什么拷贝管理器时必须创建新的 `Task` 对象，而不能直接拷贝 `unique_ptr` 的值？
3. 如果拷贝过程中分配内存失败，copy-and-swap 如何保护目标对象？
4. 移动后的源管理器应该向调用者保证什么状态？
5. 为什么任务身份由 ID 决定，而标题和完成状态仍然是可变的值？
6. 保留 W2 文件格式，如何体现这是一次重构，而不是用另一个应用替换原应用？

## 故障排查

- **拷贝无法编译，或者修改副本会改变原对象。**检查管理器的两个拷贝操作，以及 `tasks_` 中的 `unique_ptr` 所有权。副本需要独立的 `Task` 对象；比较两个 `find` 指针，并只在副本中将任务标记为完成，以检查独立性。
- **移动后目标中仍有旧任务，或源对象不为空。**检查两个移动操作，然后检查移动后目标的内容和 `source.empty()`。单独测试自移动：它必须保留管理器已有的任务。
- **`find` 能找到任务，但 `list` 为空或顺序错误。**检查 `src/task_manager.cpp` 中的 `size`、`empty` 和 `snapshot`；初始代码返回的是占位值。让它们反映实际拥有的集合，再检查 `snapshot` 是否按 ID 顺序返回值副本。
- **重构后无法加载 W2 文件。**对照上面的 `CPPAPPLAB_TODO_V1`，检查保存的版本头、以制表符分隔的字段和标题转义。使用 W2 格式的文件重试；保留其 ID 和完成标志，并确保格式错误导致的加载失败不会替换当前管理器。

## 验收要求

- [ ] Task 构造时维护其不变量。
- [ ] Task 的相等判断和排序使用稳定的 ID 身份。
- [ ] TaskManager 使用独占所有权，并通过 RAII 释放被删除的任务。
- [ ] 拷贝构造和拷贝赋值产生独立的任务对象。
- [ ] 移动构造和移动赋值转移所有权，并让源对象为空。
- [ ] 快照返回有序的值副本。
- [ ] W2 数据文件能往返保存与加载，格式保持不变。
- [ ] 格式错误的加载不会只替换管理器状态的一部分。
- [ ] CLI 保留添加、列表查询、标记完成和删除行为。
- [ ] 全部 30 个单元测试和 CLI 工作流测试通过。
