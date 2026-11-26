"""Fixed C++17 knowledge probes; never execute a request-supplied source or command.

These are independent micro-experiments, not course tests or learner answers.
Only the fixed ID and failure/repair variant are accepted by the web adapter.
"""
from __future__ import annotations

import os
import signal
import subprocess
import tempfile
from pathlib import Path
from typing import Any

MAX_LOG = 64 * 1024
TOPICS: tuple[dict[str, Any], ...] = (
    {
        "id": "linkage",
        "title": "声明、定义与链接",
        "terms": "声明 定义 链接 翻译单元 declaration definition linkage linker undefined reference unresolved external",
        "task": "w1.calculator",
        "zh": "公共接口",
        "en": "Public interface",
        "explanation": "声明让编译器知道函数的签名；定义提供实现。每个翻译单元可以独立编译，但链接阶段仍需找到签名完全匹配的定义。",
        "prediction": "只有声明、没有定义的程序会在哪个阶段失败？",
        "transfer": "把一个已实现的函数移到另一个 .cpp，并通过真实多文件构建验证。",
        "bad": "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n",
        "good": "#include <iostream>\nint answer();\nint answer() { return 42; }\nint main() { std::cout << answer() << '\\n'; }\n",
        "phase": "compile",
        "marker": "undefined reference",
    },
    {
        "id": "const-reference",
        "title": "const 与引用",
        "terms": "const 常量 引用 reference const correctness read-only assignment of read-only location",
        "task": "w1.calculator",
        "zh": "公共接口",
        "en": "Public interface",
        "explanation": "const 约束通过某个名字或引用修改对象；引用不是另一份独立对象。需要改变对象时应在接口中明确可变引用，而不是移除 const 逃避约束。",
        "prediction": "通过 const int& 修改对象会在编译阶段还是运行阶段失败？",
        "transfer": "写一个只读输入参数和一个明确可变引用参数的函数，比较调用约束。",
        "bad": "int main() { int n = 41; const int& ref = n; ref = 42; }\n",
        "good": "#include <iostream>\nint main() { int n = 41; int& ref = n; ref = 42; const int& view = n; std::cout << view << '\\n'; }\n",
        "phase": "compile",
        "marker": "read-only",
    },
    {
        "id": "ownership",
        "title": "所有权、移动与 unique_ptr",
        "terms": "unique_ptr 智能指针 所有权 RAII 拷贝 移动 copy move use of deleted function",
        "task": "w3.task-manager",
        "zh": "设计约定",
        "en": "Design contract",
        "explanation": "unique_ptr 表示独占所有权，其拷贝构造被删除。std::move 允许将所有权移动给另一个对象；移动后原 unique_ptr 可以为空，不能继续无条件解引用。",
        "prediction": "复制 unique_ptr 为什么不能通过编译？移动后哪一个指针拥有对象？",
        "transfer": "在 TaskManager 中解释深拷贝与移动的区别，不修改固定测试来绕开约束。",
        "bad": "#include <memory>\nint main() { auto a = std::make_unique<int>(42); auto b = a; return *b; }\n",
        "good": "#include <memory>\n#include <utility>\n#include <iostream>\nint main() { auto a = std::make_unique<int>(42); auto b = std::move(a); if (a || !b) return 1; std::cout << *b << '\\n'; }\n",
        "phase": "compile",
        "marker": "deleted function",
    },
    {
        "id": "template-deduction",
        "title": "模板推导与重载匹配",
        "terms": "template 模板 类型推导 重载 overload deduction no matching function conflicting types",
        "task": "w6.thread-pool",
        "zh": "API 与语义",
        "en": "API and semantics",
        "explanation": "同一个模板参数 T 在多个实参上推导时必须一致。两个不同类型可能导致推导失败；显式类型参数或适当的接口设计可以消除歧义。",
        "prediction": "同一个 T 同时由 int 和 double 推导时会发生什么？",
        "transfer": "比较显式类型参数与双模板参数接口；解释线程池 submit 的调用类型约束。",
        "bad": "template<class T> T sum(T a, T b) { return a + b; }\nint main() { return sum(20, 22.0); }\n",
        "good": "#include <iostream>\ntemplate<class T> T sum(T a, T b) { return a + b; }\nint main() { std::cout << sum<double>(20, 22.0) << '\\n'; }\n",
        "phase": "compile",
        "marker": "no matching function",
    },
    {
        "id": "iterators",
        "title": "容器与迭代器能力",
        "terms": "STL 容器 迭代器 vector list iterator std::next random access no match for operator",
        "task": "w2.todo-cli",
        "zh": "数据模型与接口",
        "en": "Data model and interface",
        "explanation": "不同容器提供不同迭代器能力。list 的双向迭代器不能用随机访问的加法；std::next 可以按迭代器支持的方式前进。不要假设所有容器都支持下标和随机访问。",
        "prediction": "std::list 的迭代器可以直接 +1 吗？",
        "transfer": "在 vector 与 list 上比较 std::next、复杂度与迭代器失效条件。",
        "bad": "#include <list>\nint main() { std::list<int> v{0,42}; return *(v.begin() + 1); }\n",
        "good": "#include <list>\n#include <iterator>\n#include <iostream>\nint main() { std::list<int> v{0,42}; std::cout << *std::next(v.begin()) << '\\n'; }\n",
        "phase": "compile",
        "marker": "no match for",
    },
    {
        "id": "bounds",
        "title": "边界检查与异常",
        "terms": "边界 越界 vector at out_of_range checked access exception 范围检查",
        "task": "w2.todo-cli",
        "zh": "数据模型与接口",
        "en": "Data model and interface",
        "explanation": "vector::at 在下标超出范围时抛出 std::out_of_range，和未检查的 operator[] 不同。本实验显式捕获异常并返回非零，不执行越界未定义行为。",
        "prediction": "只有一个元素时访问 at(1) 会发生什么？",
        "transfer": "为 Todo 查询补充空集合和不存在的 ID 边界测试。",
        "bad": "#include <vector>\n#include <stdexcept>\n#include <iostream>\nint main() { std::vector<int> v{42}; try { std::cout << v.at(1) << '\\n'; } catch (const std::out_of_range&) { std::cerr << \"caught std::out_of_range\\n\"; return 2; } return 0; }\n",
        "good": "#include <vector>\n#include <iostream>\nint main() { std::vector<int> v{42}; std::cout << v.at(0) << '\\n'; }\n",
        "phase": "run",
        "marker": "caught std::out_of_range",
    },
    {
        "id": "cmake-target",
        "title": "CMake 目标与源文件",
        "terms": "CMake cmake target source file configure generate Cannot find source file No SOURCES given 构建 配置 目标",
        "task": "w4.quality-gate",
        "zh": "CMake 与 CTest",
        "en": "CMake and CTest",
        "explanation": "CMake 先配置并生成构建系统，再编译链接目标。目标引用不存在的文件时，配置或生成会失败；修改目标源文件清单后重新配置、构建和运行。",
        "prediction": "不存在的源文件在配置/生成阶段还是程序运行阶段报错？",
        "transfer": "为多文件计算器核对 add_executable 源文件与声明定义的关系。",
        "bad": "cmake_minimum_required(VERSION 3.18)\nproject(KnowledgeLab LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\nadd_executable(probe missing.cpp)\n",
        "good": "cmake_minimum_required(VERSION 3.18)\nproject(KnowledgeLab LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 17)\nadd_executable(probe main.cpp)\n",
        "phase": "configure",
        "marker": "Cannot find source file",
    },
    {
        "id": "lifetime-sanitizers",
        "title": "对象生命周期与 Sanitizer",
        "terms": "生命周期 指针 悬空引用 dangling pointer lifetime AddressSanitizer UndefinedBehaviorSanitizer ASan UBSan heap-use-after-free stack-use-after-scope segmentation fault",
        "task": "w4.quality-gate",
        "zh": "Sanitizer 与内存诊断",
        "en": "Sanitizers and memory diagnostics",
        "explanation": "对象生命周期结束后使用旧指针或引用可能产生未定义行为。ASan/UBSan 提供诊断，但没有报错并不能证明程序安全；要检查所有权、释放与作用域。",
        "prediction": "没有出现崩溃是否能证明悬空指针安全？",
        "transfer": "按已有 W4 讲义在受控质量门禁中检查生命周期与异常路径。此知识点不在网页执行故意 UB。",
        "phase": None,
    },
    {
        "id": "concurrency",
        "title": "互斥、条件变量与并发边界",
        "terms": "mutex lock_guard unique_lock condition_variable 线程 并发 死锁 数据竞争 race backpressure synchronization",
        "task": "w5.system-lab",
        "zh": "并发模型",
        "en": "Concurrency model",
        "explanation": "共享状态需要与明确的同步契约配合。条件变量需要谓词；等待和唤醒不是工作完成的证明。锁顺序、超时和关闭语义应通过固定边界测试验证。",
        "prediction": "为什么条件变量应在谓词循环中等待？",
        "transfer": "在已有 W5/W6 合同中解释队列满、关闭与生产者消费者行为。此项先阅读已有真实工程。",
        "phase": None,
    },
    {
        "id": "scope",
        "title": "作用域与局部变量",
        "terms": "scope block local variable 作用域 局部变量 not declared in this scope",
        "task": "w1.calculator",
        "zh": "独立基础语法补课",
        "en": "Independent language fundamentals",
        "explanation": "块内声明的局部变量在块外不可见。把需要跨块使用的结果定义在正确的外层作用域，而不是使用不必要的全局变量。",
        "prediction": "代码块结束以后，块内的 value 还能访问吗？",
        "transfer": "把计算器某个分支的局部结果调整到正确的作用域。",
        "bad": "int main() { { int value = 42; } return value; }\n",
        "good": "#include <iostream>\nint main() { int value = 0; { value = 42; } std::cout << value << '\\n'; }\n",
        "phase": "compile",
        "marker": "not declared in this scope",
    },
    {
        "id": "narrowing",
        "title": "列表初始化与窄化转换",
        "terms": "narrowing list initialization brace conversion 列表初始化 窄化 转换",
        "task": "w1.calculator",
        "zh": "独立基础语法补课",
        "en": "Independent language fundamentals",
        "explanation": "列表初始化禁止某些可能丢失信息的隐式窄化转换，例如 double 到 int。先根据物理量或接口选择正确类型，再显式表达确实需要的转换。",
        "prediction": "int value{42.5} 会隐式舍去小数吗？",
        "transfer": "比较 double 输入、int 输出与显式转换的语义，避免静默精度丢失。",
        "bad": "int main() { int value{42.5}; return value; }\n",
        "good": "#include <iostream>\nint main() { double value{42.0}; std::cout << value << '\\n'; }\n",
        "phase": "compile",
        "marker": "narrowing conversion",
    },
    {
        "id": "value-reference",
        "title": "值传递与引用传递",
        "terms": "pass by value pass by reference copy alias 值传递 引用传递 别名",
        "task": "w1.calculator",
        "zh": "独立基础语法补课",
        "en": "Independent language fundamentals",
        "explanation": "按值传递修改的是参数副本；可变引用修改原对象。失败示例有明确定义的行为，只是没有满足修改调用者对象的合同。",
        "prediction": "bump(int value) 调用后原变量为什么仍为 41？",
        "transfer": "分别实现不修改输入的计算函数和显式修改对象的函数。",
        "bad": "#include <iostream>\nvoid bump(int value) { ++value; }\nint main() { int n = 41; bump(n); if (n != 42) { std::cerr << \"observed \" << n << '\\n'; return 2; } std::cout << n << '\\n'; }\n",
        "good": "#include <iostream>\nvoid bump(int& value) { ++value; }\nint main() { int n = 41; bump(n); if (n != 42) { std::cerr << \"observed \" << n << '\\n'; return 2; } std::cout << n << '\\n'; }\n",
        "phase": "run",
        "marker": "observed 41",
    },
    {
        "id": "scoped-enum",
        "title": "强类型枚举与显式转换",
        "terms": "enum class scoped enumeration 隔离枚举 强类型枚举 cannot convert",
        "task": "w1.calculator",
        "zh": "独立基础语法补课",
        "en": "Independent language fundamentals",
        "explanation": "enum class 的枚举值不会隐式转换为整数。优先比较有类型的枚举值；序列化或日志需要数字时才在明确边界显式转换。",
        "prediction": "把 Operation::Add 直接交给 int 会发生什么？",
        "transfer": "在计算器接口中比较枚举错误值，不依赖魔法数字。",
        "bad": "enum class Code { Answer = 42 };\nint main() { int n = Code::Answer; return n; }\n",
        "good": "#include <iostream>\nenum class Code { Answer = 42 };\nint main() { std::cout << static_cast<int>(Code::Answer) << '\\n'; }\n",
        "phase": "compile",
        "marker": "cannot convert",
    },
    {
        "id": "lambda-capture",
        "title": "Lambda 捕获与谓词",
        "terms": "lambda capture predicate find_if 捕获 谓词 not captured",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "Lambda 不会自动捕获所有外部局部变量。根据是否需要修改或共享对象选值捕获或引用捕获，并注意对象生命周期。",
        "prediction": "空捕获列表能读取外层 target 吗？",
        "transfer": "用 find_if 构造一个查找 Todo ID 的只读谓词。",
        "bad": "int main() { int target = 42; auto f = [] { return target; }; return f(); }\n",
        "good": "#include <algorithm>\n#include <iostream>\n#include <vector>\nint main() { int target = 42; const std::vector<int> v{1,42}; auto it = std::find_if(v.begin(), v.end(), [target](int n) { return n == target; }); if (it == v.end()) return 1; std::cout << *it << '\\n'; }\n",
        "phase": "compile",
        "marker": "not captured",
    },
    {
        "id": "fixed-array",
        "title": "std::array 与固定长度",
        "terms": "array fixed size aggregate 固定数组 容量 初始化 too many initializers",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "std::array<T,N> 的元素数属于类型；初始化器不能超过容量。固定数组适合大小在编译期明确的数据，不应伪装成可动态增长的容器。",
        "prediction": "长度为 1 的 array 能用两个元素初始化吗？",
        "transfer": "比较固定长度采样窗与动态 Todo 列表的容器选择。",
        "bad": "#include <array>\nint main() { std::array<int,1> a{1,42}; return a[0]; }\n",
        "good": "#include <array>\n#include <iostream>\nint main() { const std::array<int,2> a{1,42}; std::cout << a.at(1) << '\\n'; }\n",
        "phase": "compile",
        "marker": "too many initializers",
    },
    {
        "id": "map-lookup",
        "title": "map 查找与隐式插入",
        "terms": "map unordered_map find operator[] 插入 查找 关联容器",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "对不存在的 key 使用 map::operator[] 会插入默认值，而 find 不会。若操作约定只读查找，使用 find 并判断 end。",
        "prediction": "查询不存在的 key 后 map 的 size 是否改变？",
        "transfer": "实现查找不存在 ID 时保持集合不变，并说明返回错误与创建新项的区别。",
        "bad": "#include <map>\n#include <iostream>\nint main() { std::map<int,int> m{{1,42}}; (void)m[2]; if (m.size() != 1) { std::cerr << \"observed size=\" << m.size() << '\\n'; return 2; } std::cout << m.at(1) << '\\n'; }\n",
        "good": "#include <map>\n#include <iostream>\nint main() { std::map<int,int> m{{1,42}}; if (m.find(2) != m.end() || m.size() != 1) return 2; std::cout << m.at(1) << '\\n'; }\n",
        "phase": "run",
        "marker": "observed size=2",
    },
    {
        "id": "optional",
        "title": "optional 与显式空值",
        "terms": "optional nullopt bad_optional_access 空值 可选值",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "optional 将可能缺失的值表示为显式状态。value() 在空状态抛异常，value_or 提供调用方指定的默认值。不要对缺失值直接解引用。",
        "prediction": "空 optional 调用 value() 会发生什么？",
        "transfer": "将可缺失查询结果与错误状态区分，并检查默认值是否符合业务要求。",
        "bad": "#include <optional>\n#include <iostream>\nint main() { std::optional<int> v; try { std::cout << v.value() << '\\n'; } catch (const std::bad_optional_access&) { std::cerr << \"caught bad_optional_access\\n\"; return 2; } return 0; }\n",
        "good": "#include <optional>\n#include <iostream>\nint main() { std::optional<int> v; std::cout << v.value_or(42) << '\\n'; }\n",
        "phase": "run",
        "marker": "caught bad_optional_access",
    },
    {
        "id": "variant",
        "title": "variant 与类型安全联合",
        "terms": "variant get_if bad_variant_access visit 类型安全联合 多类型",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "variant 在任一时刻保存一个备选类型。std::get<T> 在当前类型不匹配时抛异常；std::get_if 返回空指针供检查。不能假定当前类型。",
        "prediction": "当前保存 string 时 get<int> 会怎样？",
        "transfer": "把两类输入状态建模为 variant 并在分支里检查当前类型。",
        "bad": "#include <variant>\n#include <string>\n#include <iostream>\nint main() { std::variant<int,std::string> v{std::string(\"42\")}; try { std::cout << std::get<int>(v) << '\\n'; } catch (const std::bad_variant_access&) { std::cerr << \"caught bad_variant_access\\n\"; return 2; } return 0; }\n",
        "good": "#include <variant>\n#include <string>\n#include <iostream>\nint main() { std::variant<int,std::string> v{42}; const int* p = std::get_if<int>(&v); if (!p) return 1; std::cout << *p << '\\n'; }\n",
        "phase": "run",
        "marker": "caught bad_variant_access",
    },
    {
        "id": "virtual-override",
        "title": "继承、虚函数与 override",
        "terms": "inheritance virtual override polymorphism 继承 虚函数 多态 does not override",
        "task": "w3.task-manager",
        "zh": "独立对象模型补课",
        "en": "Independent object-model practice",
        "explanation": "基类虚函数与派生类覆盖的签名必须匹配，包括 const 限定。override 让编译器检查覆盖关系；通过基类引用调用虚函数可观察动态分派。",
        "prediction": "基类的 const 虚函数能被非 const 的同名成员覆盖吗？",
        "transfer": "设计两个小型行为不同的派生类型，通过同一基类接口调用；不强改现有 Task 的值语义。",
        "bad": "struct Base { virtual int value() const { return 0; } virtual ~Base() = default; };\nstruct Derived : Base { int value() override { return 42; } };\nint main() { Derived d; return d.value(); }\n",
        "good": "#include <iostream>\nstruct Base { virtual int value() const { return 0; } virtual ~Base() = default; };\nstruct Derived : Base { int value() const override { return 42; } };\nint main() { Derived d; const Base& b = d; std::cout << b.value() << '\\n'; }\n",
        "phase": "compile",
        "marker": "does not override",
    },
    {
        "id": "abstract-interface",
        "title": "抽象类与纯虚接口",
        "terms": "abstract class pure virtual interface 抽象类 纯虚函数 cannot declare variable abstract type",
        "task": "w3.task-manager",
        "zh": "独立对象模型补课",
        "en": "Independent object-model practice",
        "explanation": "含未实现纯虚函数的类不能直接实例化。派生类实现全部要求后成为具体类型。抽象接口用于行为契约，不是所有类都需要继承。",
        "prediction": "纯虚接口能直接构造对象吗？",
        "transfer": "比较一个纯虚接口与一个值对象，说明何时使用哪种设计。",
        "bad": "struct Interface { virtual int value() const = 0; virtual ~Interface() = default; };\nint main() { Interface object; return object.value(); }\n",
        "good": "#include <iostream>\nstruct Interface { virtual int value() const = 0; virtual ~Interface() = default; };\nstruct Concrete final : Interface { int value() const override { return 42; } };\nint main() { Concrete object; const Interface& view = object; std::cout << view.value() << '\\n'; }\n",
        "phase": "compile",
        "marker": "abstract type",
    },
    {
        "id": "weak-ownership",
        "title": "shared_ptr 与 weak_ptr 生命周期",
        "terms": "shared_ptr weak_ptr lock expired 共享所有权 弱引用 生命周期",
        "task": "w3.task-manager",
        "zh": "独立对象模型补课",
        "en": "Independent object-model practice",
        "explanation": "shared_ptr 共享对象的所有权，weak_ptr 不延长生命周期；lock 尝试获得有效 shared_ptr。原 owner 消失后必须处理空结果，不能使用悬空对象。",
        "prediction": "唯一 shared_ptr 离开作用域后 weak_ptr.lock() 是否成功？",
        "transfer": "画出拥有者和观察者关系，说明 TaskManager 为什么仍应使用独占所有权。",
        "bad": "#include <memory>\n#include <iostream>\nint main() { std::weak_ptr<int> view; { auto owner = std::make_shared<int>(42); view = owner; } if (auto current = view.lock()) { std::cout << *current << '\\n'; return 0; } std::cerr << \"owner expired\\n\"; return 2; }\n",
        "good": "#include <memory>\n#include <iostream>\nint main() { auto owner = std::make_shared<int>(42); std::weak_ptr<int> view = owner; if (auto current = view.lock()) { std::cout << *current << '\\n'; return 0; } return 2; }\n",
        "phase": "run",
        "marker": "owner expired",
    },
    {
        "id": "constexpr",
        "title": "constexpr 与编译期断言",
        "terms": "constexpr static_assert compile time constant 编译期 常量 断言",
        "task": "w6.thread-pool",
        "zh": "独立编译期与并发原语补课",
        "en": "Independent compile-time and concurrency primitives",
        "explanation": "constexpr 函数可以用于常量表达式，但并非每次调用都在编译期执行。static_assert 在编译时检查常量条件，而运行期输入需要另行检查。",
        "prediction": "加法结果不符合 static_assert 时会在哪个阶段失败？",
        "transfer": "给固定容量与类型属性增加可解释的编译期约束。",
        "bad": "constexpr int twice(int n) { return n * 2; }\nstatic_assert(twice(20) == 42, \"compile-time contract\");\nint main() { return 0; }\n",
        "good": "#include <iostream>\nconstexpr int twice(int n) { return n * 2; }\nstatic_assert(twice(21) == 42, \"compile-time contract\");\nint main() { std::cout << twice(21) << '\\n'; }\n",
        "phase": "compile",
        "marker": "static assertion failed",
    },
    {
        "id": "atomic-access",
        "title": "atomic 的显式读写",
        "terms": "atomic load fetch_add atomic int 内存模型 原子 读写 memory order",
        "task": "w6.thread-pool",
        "zh": "独立编译期与并发原语补课",
        "en": "Independent compile-time and concurrency primitives",
        "explanation": "std::atomic<T> 本身不可复制；load 读取值，fetch_add 做原子读改写。原子计数不自动保护多个变量的不变量。本实验不证明多线程内存序或无锁算法。",
        "prediction": "auto copy = atomic_counter 为什么无法复制？",
        "transfer": "用 atomic 计数单项指标，同时说明多字段一致性为什么仍可能需要锁。",
        "bad": "#include <atomic>\nint main() { std::atomic<int> count{41}; auto copy = count; return copy.load(); }\n",
        "good": "#include <atomic>\n#include <iostream>\nint main() { std::atomic<int> count{41}; count.fetch_add(1); std::cout << count.load() << '\\n'; }\n",
        "phase": "compile",
        "marker": "deleted function",
    },
    {
        "id": "control-flow",
        "title": "分支、循环与函数边界",
        "terms": "if else for while control flow loop off by one function 分支 循环 边界 函数",
        "task": "w1.calculator",
        "zh": "独立基础语法补课",
        "en": "Independent language fundamentals",
        "explanation": "把循环与条件放在小函数中，先明确边界是否包含末项。负例是有定义的计算错误：遗漏上界的偶数 12；正确结果应为 42。",
        "prediction": "求 1 到 12 的偶数和时，i < limit 与 i <= limit 有什么差别？",
        "transfer": "修改上界为奇数、零或负数，先给出期望结果再编写独立测试。",
        "bad": "#include <iostream>\nint sum_even(int limit) {\n    int total = 0;\n    for (int i = 1; i < limit; ++i) {\n        if (i % 2 == 0) total += i;\n    }\n    return total;\n}\nint main() {\n    const int result = sum_even(12);\n    if (result != 42) {\n        std::cerr << \"observed sum=\" << result << '\\n';\n        return 2;\n    }\n    std::cout << result << '\\n';\n}\n",
        "good": "#include <iostream>\nint sum_even(int limit) {\n    int total = 0;\n    for (int i = 1; i <= limit; ++i) {\n        if (i % 2 == 0) total += i;\n    }\n    return total;\n}\nint main() {\n    const int result = sum_even(12);\n    if (result != 42) {\n        std::cerr << \"observed sum=\" << result << '\\n';\n        return 2;\n    }\n    std::cout << result << '\\n';\n}\n",
        "phase": "run",
        "marker": "observed sum=30",
    },
    {
        "id": "string-view",
        "title": "string_view 非拥有视图与生命周期",
        "terms": "std::string_view string view lifetime non-owning alias snapshot 字符串 视图 生命周期 非拥有",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "string_view 不拥有字符存储，也不是值快照。负例保持原字符串存活且不重分配，只改一个字符，观察视图随之变化。若需要独立保存内容，应复制成 string；不得访问已悬空视图。",
        "prediction": "原字符串从 42 改成 41 时，保存的 view 是否仍是 42？",
        "transfer": "检查函数返回的字符串视图由哪个对象支撑；需要跨所有者寿命保存时使用拥有型字符串。",
        "bad": "#include <iostream>\n#include <string>\n#include <string_view>\nint main() {\n    std::string owner{\"42\"};\n    std::string_view view{owner};\n    const std::string_view saved{view};\n    owner[1] = '1'; // The owner stays alive and its storage is not reallocated.\n    if (saved != \"42\") {\n        std::cerr << \"saved view now=\" << saved << '\\n';\n        return 2;\n    }\n    std::cout << saved << '\\n';\n}\n",
        "good": "#include <iostream>\n#include <string>\n#include <string_view>\nint main() {\n    std::string owner{\"42\"};\n    std::string_view view{owner};\n    const std::string saved{view};\n    owner[1] = '1'; // The owner stays alive and its storage is not reallocated.\n    if (saved != \"42\") {\n        std::cerr << \"saved view now=\" << saved << '\\n';\n        return 2;\n    }\n    std::cout << saved << '\\n';\n}\n",
        "phase": "run",
        "marker": "saved view now=41",
    },
    {
        "id": "template-specialization",
        "title": "类模板特化",
        "terms": "class template specialization partial specialization explicit specialization primary template 类模板 特化 偏特化 全特化",
        "task": "w6.thread-pool",
        "zh": "独立编译期与并发原语补课",
        "en": "Independent compile-time and concurrency primitives",
        "explanation": "主模板提供通用实现，显式特化替换某个完整类型的实现，偏特化匹配一类类型。本实验同时检查主模板、指针偏特化和 int 显式特化；缺失特化由真实 static_assert 拒绝。",
        "prediction": "Answer<int> 与 Answer<int*> 分别匹配哪一个定义？",
        "transfer": "为不同输入类型写小型 trait，先用 static_assert 验证匹配，再考虑模板接口扩展。",
        "bad": "#include <iostream>\ntemplate<class T> struct Answer { static constexpr int value = 0; };\n\ntemplate<class T> struct Answer<T*> { static constexpr int value = 7; };\nstatic_assert(Answer<double>::value == 0, \"primary template\");\nstatic_assert(Answer<int*>::value == 7, \"partial specialization\");\nstatic_assert(Answer<int>::value == 42, \"missing explicit specialization\");\nint main() { std::cout << Answer<int>::value << '\\n'; }\n",
        "good": "#include <iostream>\ntemplate<class T> struct Answer { static constexpr int value = 0; };\ntemplate<> struct Answer<int> { static constexpr int value = 42; };\ntemplate<class T> struct Answer<T*> { static constexpr int value = 7; };\nstatic_assert(Answer<double>::value == 0, \"primary template\");\nstatic_assert(Answer<int*>::value == 7, \"partial specialization\");\nstatic_assert(Answer<int>::value == 42, \"missing explicit specialization\");\nint main() { std::cout << Answer<int>::value << '\\n'; }\n",
        "phase": "compile",
        "marker": "missing explicit specialization",
    },
    {
        "id": "type-traits",
        "title": "类型特征与 SFINAE",
        "terms": "type traits sfinae enable_if is_integral substitution failure 类型特征 替换失败 模板",
        "task": "w6.thread-pool",
        "zh": "独立编译期与并发原语补课",
        "en": "Independent compile-time and concurrency primitives",
        "explanation": "std::is_integral_v 是编译期类型谓词，enable_if 在条件为假时使该模板候选不参与重载。替换失败不总是硬错误；没有可用候选时调用仍然报错。",
        "prediction": "只允许整数的 twice 为什么不能接收 double？",
        "transfer": "增加一个适合浮点数的独立重载，并对整数、浮点和不支持类型分别验证。",
        "bad": "#include <iostream>\n#include <type_traits>\ntemplate<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>\nint twice(T value) { return value * 2; }\nint main() { std::cout << twice(21.0) << '\\n'; }\n",
        "good": "#include <iostream>\n#include <type_traits>\ntemplate<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>\nint twice(T value) { return value * 2; }\nint main() { std::cout << twice(21) << '\\n'; }\n",
        "phase": "compile",
        "marker": "no matching function",
    },
    {
        "id": "memory-order",
        "title": "release/acquire 与发布顺序",
        "terms": "memory order release acquire relaxed happens before atomic publication 内存序 先行发生 发布 同步",
        "task": "w6.thread-pool",
        "zh": "独立编译期与并发原语补课",
        "en": "Independent compile-time and concurrency primitives",
        "explanation": "release 写入和读取该值的 acquire 建立同步关系，发布前的写入应对获取方可见。负例故意先发布 ready，再等待消费者确认才写 payload；全部共享通信采用原子，确定性观察旧值 0，无数据竞争。修复先写 payload 再 release。此对照证明发布顺序的重要性，不声称 relaxed 必然失败或验证所有硬件内存模型。",
        "prediction": "如果先置 ready 再写数据，即便 ready 使用 release/acquire，消费者一定能读到新数据吗？",
        "transfer": "画出两个线程的 happens-before 链；指出哪些操作受同步保护，何时应优先使用互斥锁。",
        "bad": "#include <atomic>\n#include <iostream>\n#include <thread>\nint main() {\n    std::atomic<int> payload{0};\n    std::atomic<bool> ready{false}, received{false};\n    int seen = -1;\n    std::thread producer([&] {\n        \n        ready.store(true, std::memory_order_release);\n        while (!received.load(std::memory_order_acquire)) std::this_thread::yield();\n        payload.store(42, std::memory_order_relaxed);\n    });\n    std::thread consumer([&] {\n        while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();\n        seen = payload.load(std::memory_order_relaxed);\n        received.store(true, std::memory_order_release);\n    });\n    producer.join();\n    consumer.join();\n    if (seen != 42) {\n        std::cerr << \"published before payload: \" << seen << '\\n';\n        return 2;\n    }\n    std::cout << seen << '\\n';\n}\n",
        "good": "#include <atomic>\n#include <iostream>\n#include <thread>\nint main() {\n    std::atomic<int> payload{0};\n    std::atomic<bool> ready{false}, received{false};\n    int seen = -1;\n    std::thread producer([&] {\n        payload.store(42, std::memory_order_relaxed);\n        ready.store(true, std::memory_order_release);\n        while (!received.load(std::memory_order_acquire)) std::this_thread::yield();\n        \n    });\n    std::thread consumer([&] {\n        while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();\n        seen = payload.load(std::memory_order_relaxed);\n        received.store(true, std::memory_order_release);\n    });\n    producer.join();\n    consumer.join();\n    if (seen != 42) {\n        std::cerr << \"published before payload: \" << seen << '\\n';\n        return 2;\n    }\n    std::cout << seen << '\\n';\n}\n",
        "phase": "run",
        "marker": "published before payload: 0",
    },
    {
        "id": "iterator-invalidation",
        "title": "vector 扩容与迭代器失效",
        "terms": "vector reserve capacity iterator invalidation reallocation 迭代器 失效 扩容 容量",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "reserve 超过现有容量时会重新分配，旧迭代器失效。负例在实际容量增长后报告不能继续使用旧迭代器的合同，并不访问失效迭代器。修复提前保存索引并重新取得迭代器；插入删除还可能改变索引语义。",
        "prediction": "reserve(capacity + 1) 后可以继续使用此前保存的迭代器吗？",
        "transfer": "对 Todo 列表区分重新分配与删除元素的影响；删除后不要把旧索引视为原对象身份。",
        "bad": "#include <iostream>\n#include <vector>\nint main() {\n    std::vector<int> values{1, 42};\n    const auto capacity = values.capacity();\n    // An iterator obtained now would be invalidated by a successful reserve below.\n    values.reserve(capacity + 1);\n    if (values.capacity() > capacity) {\n        std::cerr << \"capacity grew; reacquire iterator\\n\";\n        return 2; // Detect the broken retained-iterator contract, before any invalid access.\n    }\n    return 3; // Unexpected: reserve(capacity + 1) must grow capacity.\n}\n",
        "good": "#include <iostream>\n#include <vector>\nint main() {\n    std::vector<int> values{1, 42};\n    const auto before = values.cbegin() + 1;\n    const auto index = before - values.cbegin();\n    const auto capacity = values.capacity();\n    values.reserve(capacity + 1);\n    // Never access or compare before after the reallocation.\n    if (values.capacity() <= capacity) return 3;\n    const auto current = values.cbegin() + index;\n    std::cout << *current << '\\n';\n}\n",
        "phase": "run",
        "marker": "capacity grew; reacquire iterator",
    },
    {
        "id": "lookup-complexity",
        "title": "线性查找与有序二分的比较次数",
        "terms": "complexity linear logarithmic lower_bound find_if sorted vector 比较次数 复杂度 二分 查找",
        "task": "w2.todo-cli",
        "zh": "独立 STL 与值类型补课",
        "en": "Independent STL and value-type practice",
        "explanation": "在固定的 64 个有序整数中查找 42，线性遍历与二分查找都可正确定位。为这个实际例子限定 7 次比较预算，线性例因超过预算而失败，lower_bound 修复。比较次数不是计时基准；非随机访问迭代器的移动复杂度可能不同。",
        "prediction": "两个算法找到同一个元素，比较次数会相同吗？二分查找依赖什么前提？",
        "transfer": "改变长度及目标位置，记录比较次数；解释为什么无序数据不能直接套用 lower_bound。",
        "bad": "#include <algorithm>\n#include <iostream>\n#include <numeric>\n#include <vector>\nint main() {\n    std::vector<int> values(64);\n    std::iota(values.begin(), values.end(), 0);\n    int comparisons = 0;\n    auto it = std::find_if(values.begin(), values.end(),\n        [&](int n) { ++comparisons; return n == 42; });\n    if (it == values.end() || *it != 42) return 3;\n    if (comparisons > 7) {\n        std::cerr << \"comparison budget exceeded: \" << comparisons << '\\n';\n        return 2;\n    }\n    std::cout << *it << '\\n';\n}\n",
        "good": "#include <algorithm>\n#include <iostream>\n#include <numeric>\n#include <vector>\nint main() {\n    std::vector<int> values(64);\n    std::iota(values.begin(), values.end(), 0);\n    int comparisons = 0;\n    auto it = std::lower_bound(values.begin(), values.end(), 42,\n        [&](int a, int b) { ++comparisons; return a < b; });\n    if (it == values.end() || *it != 42) return 3;\n    if (comparisons > 7) {\n        std::cerr << \"comparison budget exceeded: \" << comparisons << '\\n';\n        return 2;\n    }\n    std::cout << *it << '\\n';\n}\n",
        "phase": "run",
        "marker": "comparison budget exceeded:",
    },
    {
        "id": "gdb",
        "title": "GDB 断点、单步与观察",
        "terms": "gdb debugger breakpoint backtrace next step print 调试 断点 单步 调用栈",
        "task": "w4.quality-gate",
        "zh": "使用 GDB 调试",
        "en": "Debugging with GDB",
        "explanation": "先复现错误，再在关键语句设置断点，观察变量与调用栈。GDB 是观察运行状态的工具，不是正式验收；现有 W4 讲义提供真实命令，本面板不自动启动调试器。",
        "prediction": "断点停住时，如何区分实际值、期望值与导致差异的语句？",
        "transfer": "按 W4 的 GDB 操作说明记录断点、变量值与根因；尚未收集真人调试记录。",
        "phase": None,
    },
    {
        "id": "test-design",
        "title": "测试设计与边界反例",
        "terms": "GoogleTest CTest arrange act assert regression boundary negative test 测试设计 边界 断言 反例",
        "task": "w4.quality-gate",
        "zh": "测试驱动流程",
        "en": "Test-driven workflow",
        "explanation": "将准备、执行、断言分开，测试正常、边界及异常行为。先让测试发现真实缺陷，再修复并运行全部固定验收；通过测试不能证明断言充分。",
        "prediction": "只验证返回码而不检查对象状态，会漏掉什么错误？",
        "transfer": "使用 W4 原有 learner GoogleTest，独立设计一个反例、记录失败与修复。此阅读项不伪造测试设计评分。",
        "phase": None,
    },
)
BY_ID = {topic["id"]: topic for topic in TOPICS}


def catalog(task_ids: set[str]) -> list[dict[str, Any]]:
    # The task registry remains authoritative, including in partial installations.
    return [dict(topic, course_available=topic["task"] in task_ids)
            for topic in TOPICS]


def execute(command: list[str], cwd: Path, timeout: float) -> dict[str, Any]:
    # All commands and files are selected below from immutable platform probes.
    # Bound runtime and collected output; terminate the compiler's process group.
    env = os.environ.copy()
    env.update(LC_ALL="C", LANG="C")
    with tempfile.TemporaryFile(mode="w+b") as log:
        try:
            process = subprocess.Popen(
                command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
                close_fds=True,
            )
        except OSError as exc:
            return {"command": command, "exit_code": None, "output": str(exc),
                    "timed_out": False, "unavailable": True}
        timed_out = False
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            code = process.wait(timeout=3)
        log.seek(0)
        data = log.read(MAX_LOG + 1)
    return {"command": command, "exit_code": code, "timed_out": timed_out,
            "unavailable": False, "output": data[:MAX_LOG].decode("utf-8", errors="replace") +
            ("\n[output truncated]" if len(data) > MAX_LOG else "")}


def run(topic_id: str, variant: str) -> dict[str, Any]:
    topic = BY_ID[topic_id]
    if not topic["phase"] or variant not in {"failure", "repair"}:
        raise ValueError("Unknown runnable experiment or variant.")
    expected_failure = variant == "failure"
    steps = []
    with tempfile.TemporaryDirectory(prefix="cppapplab-knowledge-") as directory:
        root = Path(directory)
        source = topic["bad" if expected_failure else "good"]
        if topic_id == "cmake-target":
            (root / "CMakeLists.txt").write_text(source, encoding="utf-8")
            (root / "main.cpp").write_text(
                '#include <iostream>\nint main() { std::cout << 42 << "\\n"; }\n', encoding="utf-8")
            commands = [
                ("configure", ["cmake", "-S", ".", "-B", "build", "-DCMAKE_CXX_COMPILER=g++"], 15),
                ("compile", ["cmake", "--build", "build", "--parallel", "2"], 20),
                ("run", ["./build/probe"], 3),
            ]
        else:
            (root / "main.cpp").write_text(source, encoding="utf-8")
            flags = ["-pthread"] if topic_id == "memory-order" else []
            commands = [
                ("compile", ["g++", "-std=c++17", "-Wall", "-Wextra", "-pedantic-errors",
                             "-fdiagnostics-color=never", *flags, "main.cpp", "-o", "probe"], 15),
                ("run", ["./probe"], 3),
            ]
        for phase, command, timeout in commands:
            step = execute(command, root, timeout)
            step["phase"] = phase
            steps.append(step)
            if step["exit_code"] != 0 or step["timed_out"] or step["unavailable"]:
                break
    last = steps[-1]
    usable = not last["timed_out"] and not last["unavailable"]
    if expected_failure:
        observed = bool(usable and isinstance(last["exit_code"], int) and last["exit_code"] > 0
                        and last["phase"] == topic["phase"]
                        and topic["marker"].lower() in last["output"].lower())
        expected = "expected diagnostic in " + topic["phase"] + ": " + topic["marker"]
    else:
        observed = bool(usable and last["phase"] == "run" and last["exit_code"] == 0
                        and last["output"].strip() == "42")
        expected = "compile/build succeeds; program prints 42 and exits 0"
    return {"topic": topic_id, "variant": variant, "observed": observed,
            "expected": expected, "steps": steps, "course_acceptance": False}
