# W1 · 多文件计算器

## 从这里开始

你不需要立刻理解每一个 C++ 头文件。先跟踪一个值在程序中的流转过程。

1. 打开 `src/main.cpp`，找到调用 `calculate` 的位置。
2. 打开 `include/calculator.hpp`，查看该函数的输入和输出类型。
3. 在 `src/calculator.cpp` 中实现函数；不必重写已提供的命令行前端。
4. 先测试一个简单用例：`add 2 3`。然后测试减法、乘法和除法。
5. 测试重要的错误用例：`divide 1 0`。正确的程序必须报告错误，而不是崩溃或返回一个普通数值。

**可以这样理解：**头文件描述调用者能够请求什么；源文件包含如何计算出结果。编译工具会把这两个源文件链接为一个程序。

如果编译时出现未定义引用错误，检查函数定义中的命名空间、名称、参数和返回类型是否与头文件一致。

## 目标

将第一个 C++ 程序扩展为多文件应用。将公共接口与实现分离，编译多个翻译单元，并显式处理算术错误。

## 你的任务

实现以下文件中声明的函数：

```text
include/calculator.hpp
```

将实现写在：

```text
src/calculator.cpp
```

`src/main.cpp` 中的命令行前端已提供。它会解析运算和操作数，调用你的 `calculate` 函数，并输出结果或诊断信息。

实现全部四种运算：

- `Add`
- `Subtract`
- `Multiply`
- `Divide`

除数为零时，返回 `Error::DivisionByZero`。遇到无效枚举值时，返回 `Error::InvalidOperation`。运算成功时，返回 `Error::None` 和数值结果。

## 公共接口

平台约定的接口如下：

```cpp
namespace cppapplab::calculator {

enum class Operation { Add, Subtract, Multiply, Divide };
enum class Error { None, DivisionByZero, InvalidOperation };

struct Result {
    double value{};
    Error error{Error::None};
};

Result calculate(Operation operation, double left, double right) noexcept;

}
```

学员工作区中的头文件供你查阅，并为编辑器提供辅助。平台验收会使用平台维护的接口副本来编译你的实现，因此修改学员头文件无法降低或重新定义验收要求。

## 自行构建

在学员工作区中执行：

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include \
  src/calculator.cpp src/main.cpp -o build/calculator

./build/calculator add 2 3
./build/calculator divide 7.5 2.5
./build/calculator divide 1 0
```

预期的命令行行为：

- `add 2 3` 输出 `5`，并以退出码 0 结束。
- `divide 7.5 2.5` 输出 `3`，并以退出码 0 结束。
- `divide 1 0` 向标准错误输出 `error: division by zero`，并以退出码 3 结束。

## 平台检查什么

显式运行 `cppapplab check` 命令后，平台会：

1. 为声明的学员源文件和接口文件计算指纹
2. 只暂存声明的 C++ 实现文件
3. 配置平台维护的 CMake 验收环境
4. 根据固定接口编译学员实现
5. 运行平台维护的七个 GoogleTest 用例
6. 运行两个 CLI 冒烟测试用例，其中包含除零场景
7. 记录结果、源代码指纹以及长度受限的日志

学员自己的构建文件和测试不作为验收依据。

## 解释你的设计

1. 为什么函数声明放在头文件中，而实现放在单独的 `.cpp` 文件中？
2. 为什么除零应该表示为一个错误，而不是返回任意数值？
3. 为什么平台使用自己维护的接口头文件副本进行编译？

## 故障排查

- **链接器报告对 `calculate` 的未定义引用。**将函数定义与 `include/calculator.hpp` 比较，包括命名空间和参数类型。使用上面的构建命令，同时包含 `src/calculator.cpp` 和 `src/main.cpp`，然后重新构建。
- **所有运算都输出 `0`。**检查 `src/calculator.cpp` 初始代码中的 `return {}`：它只提供默认结果，没有执行算术运算。先实现一种运算，检查 `Result::value` 和 `Result::error`，然后再添加其他运算。
- **`divide 1 0` 以退出码 3 结束。**阅读诊断信息：`error: division by zero` 正是预期的 CLI 行为。如果输出的是普通数值，检查除法分支，并通过 `Result` 返回约定的错误。
- **四种 CLI 命令正常，但无效运算测试失败。**CLI 会在调用 `calculate` 之前拒绝未知单词，而固定测试会直接传入 `static_cast<Operation>(99)`。检查函数如何处理四个已命名运算之外的值，并为这些值返回 `Error::InvalidOperation`。

## 验收要求

- [ ] 实现能以 C++17 标准编译，并将警告视为错误。
- [ ] 加、减、乘、除返回正确的数值。
- [ ] 除零返回 `Error::DivisionByZero`。
- [ ] 无效运算值返回 `Error::InvalidOperation`。
- [ ] 平台维护的七个单元测试全部通过。
- [ ] 两个命令行冒烟测试用例全部通过。
- [ ] 验收期间学员源代码没有发生变化。
