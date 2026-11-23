# W2 · 支持持久化的 Todo 命令行程序

## 从这里开始

分步构建 Todo 程序。不要试图一次完成持久化和所有命令。

1. 从空列表开始，让 `add` 能够追加一个有效条目。
2. 让 `list` 按 ID 顺序显示条目。
3. 为已存在的 ID 实现 `done` 和 `remove`。
4. 尝试未知 ID 和空标题。程序应报告错误，同时不改变其他条目。
5. 内存中的操作正常后，再添加保存和加载功能。运行程序两次，证明数据能在不同运行之间保留。

**STL 容器的作用：**`vector` 存储一组有序条目；`string` 存储标题；`find_if` 等算法负责搜索集合。开始之前不需要记住所有 STL 算法，在实现每项操作时参考接口和示例即可。

平台的固定验收包含格式错误的文件，以及写入中断或其他无效的持久化情况。先完成基本的 Todo 操作流程，再把这些情况作为后续的健壮性改进。

## 目标

构建一个小型命令行应用，在多次独立运行之间管理任务列表。本练习将 STL 作为实用的数据管理工具来介绍，并将文件持久化纳入程序行为。

你将使用：

- `std::vector`，持有有序的任务记录集合
- `std::string` 和 `std::string_view`，处理任务标题和命令输入
- `std::find_if`、`std::max_element`、`std::any_of` 等 STL 算法
- `std::unordered_map`，在加载数据时检测重复 ID
- `std::fstream` 和 `std::filesystem`，读写持久化数据

## 你的任务

在 `src/todo.cpp` 中实现 `include/todo.hpp` 声明的函数。`src/main.cpp` 中的命令行前端已提供。

CLI 支持四种操作：

| 命令 | 行为 |
|---|---|
| `todo add <title>` | 添加任务，并分配下一个 ID |
| `todo list` | 按 ID 顺序显示所有任务 |
| `todo done <id>` | 将已存在的任务标记为完成 |
| `todo remove <id>` | 删除已存在的任务 |

对于包含空格的标题，使用引号将其包成一个 shell 参数。

## 数据模型与接口

平台固定的接口如下：

```cpp
namespace cppapplab::todo {

enum class Error { None, InvalidId, InvalidTitle, DuplicateId, NotFound, InvalidFormat, IoError };

struct Item {
    std::uint64_t id{};
    std::string title;
    bool completed{false};
};

using List = std::vector<Item>;

struct Result {
    Error error{Error::None};
    std::string message;
    bool ok() const noexcept;
};

std::uint64_t next_id(const List& items) noexcept;
Result add(List& items, std::uint64_t id, std::string_view title);
Result mark_done(List& items, std::uint64_t id);
Result remove(List& items, std::uint64_t id);
Result save(const List& items, const std::filesystem::path& path);
Result load(List& items, const std::filesystem::path& path);

}
```

学员工作区中的头文件用于为编辑器提供辅助。平台验收构建使用自己维护的固定接口副本。修改学员头文件无法改变验收约定。

## 持久化格式

数据文件采用 UTF-8 文本，并带有版本头：

```text
CPPAPPLAB_TODO_V1
1<TAB>0<TAB>Review STL containers
2<TAB>1<TAB>Finish the first exercise
```

每条记录包含一个 ID、一个完成标志（`0` 或 `1`）和一个标题，字段之间用制表符分隔。为了保留标题中的控制字符，应分别将反斜杠、制表符、换行符和回车符编码为 `\\`、`\t`、`\n` 和 `\r`。

加载器必须拒绝格式错误的记录、无效 ID、无效完成标志、重复 ID，以及不支持的转义序列。先解析到临时列表中；只有整个文件都完成读取和验证后，才能替换调用者的列表。加载失败不得破坏调用者已有的内存数据。

平台约定文件大小不超过 1 MiB，每个标题不超过 4,096 字节。数据路径必须指向普通文件，不能是符号链接。保存时应先写入临时文件，再通过重命名将其发布到目标路径，避免写入中断后留下只写了一半的数据文件。

## 存储位置

默认情况下，CLI 将数据存储在：

`~/.local/share/cppapplab/todos.tsv`

设置 `CPPAPPLAB_TODO_FILE` 可以选择不同路径，便于测试，也便于维护相互独立的 Todo 列表。

## 本地构建与运行

在学员工作区中执行：

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/todo.cpp src/main.cpp -o build/todo

export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review STL containers"
./build/todo add "Implement file persistence"
./build/todo list
./build/todo done 1
./build/todo remove 2
./build/todo list
```

每条命令都会启动一个新进程。下一次调用必须重新加载文件，并显示上一次调用保存的状态。

## 平台检查什么

在 CppAppLab 仓库中显式运行验收命令：

```bash
./build/cppapplab check "/absolute/path/to/your/todo-workspace"
```

固定的验收环境会：

1. 根据平台维护的接口编译声明的学员实现
2. 运行 18 个 GoogleTest 用例，覆盖 ID、增删改查行为、验证、序列化、格式错误的文件和符号链接处理
3. 在多次独立进程调用之间运行端到端 CLI 工作流
4. 检查任务不存在时是否给出有用的错误信息和非零退出码
5. 记录源代码指纹以及长度受限的验收日志

学员自己的构建文件和测试不作为验收依据。

## 解释你的设计

1. 为什么 `std::vector` 适合这个有序集合？什么情况下映射容器更适合作为主要数据结构？
2. 为什么使用 `std::find_if` 比在每项操作中重复编写搜索循环更好？
3. 为什么 `load` 必须先解析到临时集合中，再替换现有集合？
4. 如果程序直接写入目标文件，并在写到一半时中断，可能出现什么问题？
5. 为什么版本头能让将来的文件格式变更更容易管理？

## 故障排查

- **连 `list` 都因 `not implemented` 而停止。**检查 `src/todo.cpp` 中的 `load`：提供的 `main` 会在分发任何命令之前调用它。先完成文件不存在时的处理；尚不存在的文件必须成功加载为空列表。
- **添加的任务在下一次调用时消失。**检查两条命令是否使用同一个 `CPPAPPLAB_TODO_FILE` 路径，再检查 `save` 及其写出的文件。使用同一个测试路径重试上面的添加与列表查询流程，并核对版本头和保存的记录。
- **删除后再次添加时分配了重复 ID。**检查 `next_id`；列表长度并不等于最大 ID。尝试已有 ID 为 2、9 和 4 的情况：下一个 ID 必须为 10。在重试 CLI 之前，也要检查空列表和 ID 耗尽的情况。
- **格式错误的文件替换了部分或全部现有条目。**检查 `load` 在哪里修改调用者的列表。解析和验证都应在临时状态中完成，只有整个文件成功后才能更新调用者的状态。用一条有效记录后接一条格式错误记录的文件重新测试，确认原列表得以保留。

## 验收要求

- [ ] 下一个 ID 比当前最大 ID 大一，并处理溢出情况。
- [ ] 添加操作拒绝零 ID、无效标题和重复 ID。
- [ ] 标记完成和删除操作能报告不存在的 ID，且不破坏列表。
- [ ] 保存与加载保留 ID、完成状态以及经过转义的标题内容。
- [ ] 拒绝格式错误的文件和重复 ID，且不会只替换当前列表的一部分。
- [ ] 拒绝符号链接形式的数据路径。
- [ ] CLI 的修改能在多次独立调用之间持久保存。
- [ ] 全部 18 个固定单元测试和 CLI 工作流测试通过。
- [ ] 验收期间学员源代码保持不变。
