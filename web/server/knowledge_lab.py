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
            commands = [
                ("compile", ["g++", "-std=c++17", "-Wall", "-Wextra", "-pedantic-errors",
                             "-fdiagnostics-color=never", "main.cpp", "-o", "probe"], 15),
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
