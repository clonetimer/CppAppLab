"""Optional, separate CP4 GCC 13 C++20/C++23 feature probes.

Fixed topic IDs, source, compiler path and flags. No caller-supplied toolchain,
source, path or command. GCC 13 uses the transitional c++2b dialect spelling.
"""
from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Any, Callable

COMPILER = "/usr/bin/g++-13"
NATIVE_VERSION = "16.2.0"


def native_compiler() -> Path | None:
    # Enabled only for the verified portable profile, not an arbitrary PATH compiler.
    value = os.environ.get("CPPAPPLAB_NATIVE_SYSROOT")
    if os.name != "nt" or not value:
        return None
    prefix = Path(value)
    compiler = prefix / "bin/g++.exe"
    if (not prefix.is_absolute() or not compiler.is_file() or
            not (prefix / "include/wchar.h").is_file()):
        return None
    return compiler
TOPICS = (
    {
        "id": "cpp20-concepts",
        "title": "Concepts 与约束重载",
        "terms": "C++20 concepts integral requires constraints constrained template 概念 约束",
        "standard": 20,
        "task": "w6.thread-pool",
        "zh": "现代 C++ 泛型与并发补课",
        "en": "Modern C++ generic and concurrency practice",
        "macros": [["__cpp_concepts", 201907], ["__cpp_lib_concepts", 202002]],
        "explanation": "用 std::integral 表达模板参数约束；不满足概念的候选在重载解析时被拒绝。与 C++17 enable_if 对照。",
        "prediction": "只接受整数的 twice 能否接收 double？",
        "transfer": "将自己的 SFINAE 小例子改成 Concepts，比较诊断。",
        "bad": "#include <concepts>\n#include <iostream>\ntemplate<std::integral T>\nT twice(T value) { return value * 2; }\nint main() { std::cout << twice(21.0) << '\\n'; }\n",
        "good": "#include <concepts>\n#include <iostream>\ntemplate<std::integral T>\nT twice(T value) { return value * 2; }\nint main() { std::cout << twice(21) << '\\n'; }\n",
        "phase": "compile",
        "marker": "constraints not satisfied",
        "pthread": False,
    },
    {
        "id": "cpp20-ranges",
        "title": "Ranges 与惰性视图",
        "terms": "C++20 ranges views filter lazy range pipeline 范围 视图 筛选",
        "standard": 20,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_ranges", 201911]],
        "explanation": "views::filter 构造惰性视图而不是新容器；底层范围必须保持有效。负例和修复比较两个定义良好的筛选条件。",
        "prediction": "筛选偶数与奇数后求和为什么不同？",
        "transfer": "比较传统算法和 views，不让视图超过所有者寿命。",
        "bad": "#include <ranges>\n#include <vector>\n#include <iostream>\nint main() {\n    const std::vector<int> values{1, 2, 40, 99};\n    auto selected = values | std::views::filter([](int x) { return x % 2 == 1; });\n    int sum = 0;\n    for (int x : selected) sum += x;\n    if (sum != 42) { std::cerr << \"observed sum=\" << sum << '\\n'; return 2; }\n    std::cout << sum << '\\n';\n}\n",
        "good": "#include <ranges>\n#include <vector>\n#include <iostream>\nint main() {\n    const std::vector<int> values{1, 2, 40, 99};\n    auto selected = values | std::views::filter([](int x) { return x % 2 == 0; });\n    int sum = 0;\n    for (int x : selected) sum += x;\n    if (sum != 42) { std::cerr << \"observed sum=\" << sum << '\\n'; return 2; }\n    std::cout << sum << '\\n';\n}\n",
        "phase": "run",
        "marker": "observed sum=100",
        "pthread": False,
    },
    {
        "id": "cpp20-span",
        "title": "span 与静态长度",
        "terms": "C++20 std::span extent contiguous view 连续存储 视图 长度",
        "standard": 20,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_span", 202002]],
        "explanation": "span 不拥有连续元素。静态 extent 是类型合同；长度必须匹配。span 不延长元素寿命，也不保证所有访问有运行期边界检查。",
        "prediction": "长度为 2 的 array 能构造 extent=3 的 span 吗？",
        "transfer": "为定长采样窗设计 span 接口，说明所有者和视图的有效期。",
        "bad": "#include <array>\n#include <span>\n#include <iostream>\nint main() {\n    const std::array<int, 2> values{1, 41};\n    std::span<const int, 3> view{values};\n    std::cout << view[0] + view[1] << '\\n';\n}\n",
        "good": "#include <array>\n#include <span>\n#include <iostream>\nint main() {\n    const std::array<int, 2> values{1, 41};\n    std::span<const int, 2> view{values};\n    std::cout << view[0] + view[1] << '\\n';\n}\n",
        "phase": "compile",
        "marker": "no matching function",
        "pthread": False,
    },
    {
        "id": "cpp20-jthread",
        "title": "jthread 与协作停止",
        "terms": "C++20 std::jthread stop_token request_stop cooperative cancellation 线程 停止 协作取消",
        "standard": 20,
        "task": "w6.thread-pool",
        "zh": "现代 C++ 泛型与并发补课",
        "en": "Modern C++ generic and concurrency practice",
        "macros": [["__cpp_lib_jthread", 201911]],
        "explanation": "jthread 能向接受 stop_token 的可调用对象传入令牌。request_stop 不是强制终止。使用 promise/future 握手，真实工作线程检查请求，join 后读结果。",
        "prediction": "未接收令牌的线程是否自动结束？",
        "transfer": "区分停止请求、队列排空与 join，保留有界操作。",
        "bad": "#include <future>\n#include <iostream>\n#include <stop_token>\n#include <thread>\nint main() {\n    std::promise<void> ready, release;\n    auto entered = ready.get_future();\n    auto go = release.get_future();\n    bool observed = false;\n    std::jthread worker([&]() {\n        std::stop_token token;\n        ready.set_value();\n        go.wait();\n        observed = token.stop_requested();\n    });\n    entered.wait();\n    worker.request_stop();\n    release.set_value();\n    worker.join();\n    if (!observed) { std::cerr << \"stop request not observed\\n\"; return 2; }\n    std::cout << 42 << '\\n';\n}\n",
        "good": "#include <future>\n#include <iostream>\n#include <stop_token>\n#include <thread>\nint main() {\n    std::promise<void> ready, release;\n    auto entered = ready.get_future();\n    auto go = release.get_future();\n    bool observed = false;\n    std::jthread worker([&](std::stop_token token) {\n        \n        ready.set_value();\n        go.wait();\n        observed = token.stop_requested();\n    });\n    entered.wait();\n    worker.request_stop();\n    release.set_value();\n    worker.join();\n    if (!observed) { std::cerr << \"stop request not observed\\n\"; return 2; }\n    std::cout << 42 << '\\n';\n}\n",
        "phase": "run",
        "marker": "stop request not observed",
        "pthread": True,
    },
    {
        "id": "cpp20-coroutine",
        "title": "协程挂起、恢复与句柄所有权",
        "terms": "C++20 coroutine co_yield promise handle suspend resume 协程 挂起 恢复",
        "standard": 20,
        "task": "w6.thread-pool",
        "zh": "现代 C++ 泛型与并发补课",
        "en": "Modern C++ generic and concurrency practice",
        "macros": [["__cpp_impl_coroutine", 201902], ["__cpp_lib_coroutine", 201902]],
        "explanation": "协程可在初始挂起点暂停，resume 后才执行 co_yield。promise 值初始为 0；单一 RAII 对象销毁句柄，不访问已销毁对象。",
        "prediction": "调用 generate 是否已经执行到 co_yield？",
        "transfer": "画出 initial_suspend、resume、yield、destroy 的生命周期。",
        "bad": "#include <coroutine>\n#include <exception>\n#include <iostream>\nstruct Sample {\n    struct promise_type {\n        int value = 0;\n        Sample get_return_object() {\n            return Sample{std::coroutine_handle<promise_type>::from_promise(*this)};\n        }\n        std::suspend_always initial_suspend() noexcept { return {}; }\n        std::suspend_always final_suspend() noexcept { return {}; }\n        std::suspend_always yield_value(int n) noexcept { value = n; return {}; }\n        void return_void() noexcept {}\n        void unhandled_exception() { std::terminate(); }\n    };\n    std::coroutine_handle<promise_type> handle;\n    explicit Sample(std::coroutine_handle<promise_type> h) : handle(h) {}\n    Sample(const Sample&) = delete;\n    Sample& operator=(const Sample&) = delete;\n    ~Sample() { if (handle) handle.destroy(); }\n};\nSample generate() { co_yield 42; }\nint main() {\n    auto sample = generate();\n    \n    const int value = sample.handle.promise().value;\n    if (value != 42) { std::cerr << \"observed yield=\" << value << '\\n'; return 2; }\n    std::cout << value << '\\n';\n}\n",
        "good": "#include <coroutine>\n#include <exception>\n#include <iostream>\nstruct Sample {\n    struct promise_type {\n        int value = 0;\n        Sample get_return_object() {\n            return Sample{std::coroutine_handle<promise_type>::from_promise(*this)};\n        }\n        std::suspend_always initial_suspend() noexcept { return {}; }\n        std::suspend_always final_suspend() noexcept { return {}; }\n        std::suspend_always yield_value(int n) noexcept { value = n; return {}; }\n        void return_void() noexcept {}\n        void unhandled_exception() { std::terminate(); }\n    };\n    std::coroutine_handle<promise_type> handle;\n    explicit Sample(std::coroutine_handle<promise_type> h) : handle(h) {}\n    Sample(const Sample&) = delete;\n    Sample& operator=(const Sample&) = delete;\n    ~Sample() { if (handle) handle.destroy(); }\n};\nSample generate() { co_yield 42; }\nint main() {\n    auto sample = generate();\n    sample.handle.resume();\n    const int value = sample.handle.promise().value;\n    if (value != 42) { std::cerr << \"observed yield=\" << value << '\\n'; return 2; }\n    std::cout << value << '\\n';\n}\n",
        "phase": "run",
        "marker": "observed yield=0",
        "pthread": False,
    },
    {
        "id": "cpp23-expected",
        "title": "expected 与可解释错误",
        "terms": "C++23 std::expected unexpected bad_expected_access value error 显式错误",
        "standard": 23,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_expected", 202202]],
        "explanation": "expected 表达值或错误。失败状态调用 value() 抛出 bad_expected_access；正确分支先检查状态并保留错误原因。",
        "prediction": "divide(42,0) 处于错误状态，直接 value() 会如何？",
        "transfer": "将独立状态码函数改成 expected，为错误和成功分别写测试。",
        "bad": "#include <expected>\n#include <string>\n#include <iostream>\nstd::expected<int, std::string> divide(int n, int d) {\n    if (d == 0) return std::unexpected(std::string{\"divide by zero\"});\n    return n / d;\n}\nint main() {\n    try { std::cout << divide(42, 0).value() << '\\n'; }\n    catch (const std::bad_expected_access<std::string>& error) {\n        std::cerr << \"caught bad_expected_access: \" << error.error() << '\\n';\n        return 2;\n    }\n    return 3;\n}\n",
        "good": "#include <expected>\n#include <string>\n#include <iostream>\nstd::expected<int, std::string> divide(int n, int d) {\n    if (d == 0) return std::unexpected(std::string{\"divide by zero\"});\n    return n / d;\n}\nint main() {\n    auto error = divide(42, 0);\n    if (error || error.error() != \"divide by zero\") return 3;\n    auto result = divide(84, 2);\n    if (!result) return 3;\n    std::cout << *result << '\\n';\n}\n",
        "phase": "run",
        "marker": "caught bad_expected_access: divide by zero",
        "pthread": False,
    },
    {
        "id": "cpp23-if-consteval",
        "title": "if consteval 与求值语境",
        "terms": "C++23 if consteval constant evaluation constexpr 求值语境 编译期 运行期",
        "standard": 23,
        "task": "w6.thread-pool",
        "zh": "现代 C++ 泛型与并发补课",
        "en": "Modern C++ generic and concurrency practice",
        "macros": [["__cpp_if_consteval", 202106]],
        "explanation": "if consteval 根据是否处于清单常量求值选择分支，而不只是检查参数是否为常量。静态断言和运行期调用分别验证。",
        "prediction": "相同函数在 static_assert 与普通调用一定走同一分支吗？",
        "transfer": "比较 constexpr、consteval 与 if consteval。",
        "bad": "#include <iostream>\nconstexpr int evaluate(int x) {\n    if consteval { return x * 2; }\n    else { return x + 1; }\n}\nstatic_assert(evaluate(21) == 42);\nint main() {\n    int input = 21;\n    const int result = evaluate(input);\n    if (result != 42) { std::cerr << \"runtime result=\" << result << '\\n'; return 2; }\n    std::cout << result << '\\n';\n}\n",
        "good": "#include <iostream>\nconstexpr int evaluate(int x) {\n    if consteval { return x * 2; }\n    else { return x + 1; }\n}\nstatic_assert(evaluate(21) == 42);\nint main() {\n    int input = 41;\n    const int result = evaluate(input);\n    if (result != 42) { std::cerr << \"runtime result=\" << result << '\\n'; return 2; }\n    std::cout << result << '\\n';\n}\n",
        "phase": "run",
        "marker": "runtime result=22",
        "pthread": False,
    },
    {
        "id": "cpp23-byteswap",
        "title": "byteswap 与字节序",
        "terms": "C++23 std::byteswap byte order endian protocol 整数 字节序",
        "standard": 23,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_byteswap", 202110]],
        "explanation": "byteswap 反转整数的字节顺序，不自动判断主机或网络字节序。本例使用明确的 32 位整数及输入表示，检查双反转恢复原值。",
        "prediction": "0x2a000000 能直接当作数值 42 吗？",
        "transfer": "定义网络字段转换合同，先确认协议字节序和整数宽度。",
        "bad": "#include <bit>\n#include <cstdint>\n#include <iostream>\nint main() {\n    const std::uint32_t wire = 0x2a000000U;\n    const auto decoded = wire;\n    if (decoded != 42U) { std::cerr << \"incorrect byte order=\" << decoded << '\\n'; return 2; }\n    if (std::byteswap(std::byteswap(wire)) != wire) return 3;\n    std::cout << decoded << '\\n';\n}\n",
        "good": "#include <bit>\n#include <cstdint>\n#include <iostream>\nint main() {\n    const std::uint32_t wire = 0x2a000000U;\n    const auto decoded = std::byteswap(wire);\n    if (decoded != 42U) { std::cerr << \"incorrect byte order=\" << decoded << '\\n'; return 2; }\n    if (std::byteswap(std::byteswap(wire)) != wire) return 3;\n    std::cout << decoded << '\\n';\n}\n",
        "phase": "run",
        "marker": "incorrect byte order=",
        "pthread": False,
    },
    {
        "id": "cpp23-contains",
        "title": "contains 与查找谓词",
        "terms": "C++23 std::string contains find npos string matching 字符串 包含 前缀",
        "standard": 23,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_string_contains", 202011]],
        "explanation": "find 返回位置或 npos 而不是 bool。位置 0 转 bool 为 false，漏掉前缀；contains 直接回答是否包含。",
        "prediction": "第 0 位匹配时，直接将 find 转 bool 有何问题？",
        "transfer": "为字符串查询写起始、中间、缺失边界测试。",
        "bad": "#include <string>\n#include <iostream>\nint main() {\n    const std::string text{\"42 sample\"};\n    const bool found = static_cast<bool>(text.find(\"42\"));\n    if (!found) { std::cerr << \"prefix match was lost\\n\"; return 2; }\n    if (text.contains(\"missing\")) return 3;\n    std::cout << 42 << '\\n';\n}\n",
        "good": "#include <string>\n#include <iostream>\nint main() {\n    const std::string text{\"42 sample\"};\n    const bool found = text.contains(\"42\");\n    if (!found) { std::cerr << \"prefix match was lost\\n\"; return 2; }\n    if (text.contains(\"missing\")) return 3;\n    std::cout << 42 << '\\n';\n}\n",
        "phase": "run",
        "marker": "prefix match was lost",
        "pthread": False,
    },
    {
        "id": "cpp23-underlying",
        "title": "to_underlying 与枚举边界",
        "terms": "C++23 std::to_underlying enum class underlying_type 类型安全 枚举 转换",
        "standard": 23,
        "task": "w2.todo-cli",
        "zh": "现代 C++ 数据与库补课",
        "en": "Modern C++ data and library practice",
        "macros": [["__cpp_lib_to_underlying", 202102]],
        "explanation": "to_underlying 返回枚举底层类型，不是隐式把强类型枚举变成 int。日志等明确边界再转换；uint8_t 输出时转 int 避免字符语义。",
        "prediction": "enum class 能隐式赋值给 int 吗？to_underlying 返回哪种类型？",
        "transfer": "为协议枚举明确整数宽度与有效值检查。",
        "bad": "#include <utility>\n#include <cstdint>\n#include <iostream>\nenum class Channel : std::uint8_t { Answer = 42 };\nint main() {\n    const int value = Channel::Answer;\n    std::cout << value << '\\n';\n}\n",
        "good": "#include <utility>\n#include <cstdint>\n#include <iostream>\nenum class Channel : std::uint8_t { Answer = 42 };\nint main() {\n    const int value = static_cast<int>(std::to_underlying(Channel::Answer));\n    std::cout << value << '\\n';\n}\n",
        "phase": "compile",
        "marker": "cannot convert",
        "pthread": False,
    },
)
BY_ID = {topic["id"]: topic for topic in TOPICS}


def present() -> bool:
    return Path(COMPILER).is_file() and os.access(COMPILER, os.X_OK)


def catalog(task_ids: set[str]) -> list[dict[str, Any]]:
    available = present()
    return [dict(topic, course_available=topic["task"] in task_ids,
                 toolchain_present=available) for topic in TOPICS]


def feature_source(topic: dict[str, Any]) -> str:
    minimum = 202002 if topic["standard"] == 20 else 202100
    lines = ["#include <version>",
             f"#if __cplusplus < {minimum}L",
             "#error CPPAPPLAB_FEATURE_UNAVAILABLE language_dialect", "#endif"]
    for name, value in topic["macros"]:
        lines.extend([f"#if !defined({name}) || {name} < {value}L",
                      f"#error CPPAPPLAB_FEATURE_UNAVAILABLE {name}", "#endif"])
    return "\n".join(lines) + "\n"


def run(topic_id: str, variant: str,
        execute: Callable[[list[str], Path, float], dict[str, Any]], *,
        native_step: Callable[[str, list[str], Path, float, int, bool], dict[str, Any]] | None = None
        ) -> dict[str, Any]:
    topic = BY_ID[topic_id]
    if variant not in {"failure", "repair"}:
        raise ValueError("Unknown variant.")
    steps: list[dict[str, Any]] = []
    result: dict[str, Any] = {
        "topic": topic_id, "variant": variant, "standard": topic["standard"],
        "observed": False, "course_acceptance": False, "steps": steps,
    }
    native = native_step is not None
    selected = native_compiler() if native else None
    if native and selected is None:
        return dict(result, status="TOOLCHAIN_UNAVAILABLE",
                    expected="Verified bundled Windows GCC 16.2.0 required; no fallback")
    compiler = str(selected) if native else COMPILER
    profile = "native-windows-bundled-gcc16" if native else "linux-gcc13"
    result["compiler_profile"] = profile
    with tempfile.TemporaryDirectory(prefix="cppapplab-modern-") as directory:
        root = Path(directory)

        def step(phase: str, command: list[str], timeout: float) -> dict[str, Any]:
            current = (native_step(phase, command, root, timeout, topic["standard"], topic["pthread"])
                       if native_step is not None else execute(command, root, timeout))
            current["phase"] = phase
            steps.append(current)
            return current

        def successful(current: dict[str, Any]) -> bool:
            return (current["exit_code"] == 0 and not current["timed_out"]
                    and not current["unavailable"])

        version = step("toolchain", [compiler, "-dumpfullversion", "-dumpversion"], 5)
        compiler_version = version["output"].strip()
        result["compiler_version"] = compiler_version
        version_ok = compiler_version == NATIVE_VERSION if native else compiler_version.startswith("13.")
        if not successful(version) or not version_ok:
            return dict(result, status="TOOLCHAIN_UNAVAILABLE",
                        expected=("Bundled native GCC 16.2.0 required" if native else
                                  "GCC 13.x required; no fallback to C++17"))
        if native:
            target = step("target", [compiler, "-dumpmachine"], 5)
            if not successful(target) or target["output"].strip() != "x86_64-w64-mingw32":
                return dict(result, status="TOOLCHAIN_UNAVAILABLE",
                            expected="Native x86_64-w64-mingw32 compiler target required")
        dialect = "-std=c++20" if topic["standard"] == 20 else ("-std=c++23" if native else "-std=c++2b")
        sysroot_flags = ["--sysroot=" + selected.parent.parent.as_posix()] if selected is not None else []
        (root / "feature.cpp").write_text(feature_source(topic), encoding="utf-8")
        feature = step("feature-check", [compiler, *sysroot_flags, dialect, "-fsyntax-only", "feature.cpp"], 15)
        if not successful(feature):
            return dict(result, status="FEATURE_UNAVAILABLE",
                        expected="The required dialect and feature macros must pass first")
        source = topic["bad"] if variant == "failure" else topic["good"]
        (root / "main.cpp").write_text(source, encoding="utf-8")
        flags = ["-pthread"] if topic["pthread"] else []
        executable = "probe.exe" if native else "probe"
        last = step("compile", [compiler, *sysroot_flags, dialect, "-Wall", "-Wextra", "-pedantic-errors",
                                "-fdiagnostics-color=never", *flags, "main.cpp", "-o", executable], 15)
        if successful(last):
            last = step("run", [str(root / executable) if native else "./probe"], 3)
    usable = not last["timed_out"] and not last["unavailable"]
    if variant == "failure":
        expected = "expected diagnostic in " + topic["phase"] + ": " + topic["marker"]
        observed = (usable and isinstance(last["exit_code"], int) and last["exit_code"] > 0
                    and last["phase"] == topic["phase"]
                    and topic["marker"].lower() in last["output"].lower())
    else:
        expected = "compile succeeds; actual program prints 42 and exits 0"
        observed = (usable and last["phase"] == "run" and last["exit_code"] == 0
                    and last["output"].strip() == "42")
    return dict(result, observed=bool(observed), expected=expected,
                status="OBSERVED" if observed else "NOT_OBSERVED")
