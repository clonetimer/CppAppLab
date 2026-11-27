# CP4 — 独立现代 C++ 教学配置（第一轮候选）

## 边界与使用

原 23 个正式 W1–W9 工程继续使用 GCC/CMake C++17，Ubuntu 22.04/24.04（Windows 通过 WSL2）。现代实验是独立附加知识点，不改变 CMake 默认标准、系统 g++ alternatives、课程序列、正式验收或学员源码。首个现代验证目标 Ubuntu 24.04 x86_64，固定 GCC major 13（实际 patch 版本在每次执行报告中记录，不假称精确包版本冻结）。不承诺 Ubuntu 22.04 原生提供此编译器，不启动 Windows-native 迁移。

在 Ubuntu 24.04 有网络并且需要现代实验时自行安装（不由网页执行安装）：

```bash
sudo apt-get update
sudo apt-get install g++-13
/usr/bin/g++-13 --version
```

回到知识面板刷新页面，搜索 C++20 或 C++23。工具链文件存在仅表示可尝试；每次运行实际核验 GCC 13.x 版本及指定语言标准和特性测试宏。缺少工具链或特性时显式报告不可用，绝不把错误配置当作预期负例。C++20 使用 -std=c++20；GCC 13 的 C++23 配置采用 -std=c++2b 过渡拼写，验证选择的特性，不代表完整 ISO C++23 实现。

## 首批代表性教学合同

| 标准 | 独立知识点 | 最低特性宏 |
|---|---|---|
| C++20 | cpp20-concepts · Concepts 与约束重载 | __cpp_concepts >= 201907; __cpp_lib_concepts >= 202002 |
| C++20 | cpp20-ranges · Ranges 与惰性视图 | __cpp_lib_ranges >= 201911 |
| C++20 | cpp20-span · span 与静态长度 | __cpp_lib_span >= 202002 |
| C++20 | cpp20-jthread · jthread 与协作停止 | __cpp_lib_jthread >= 201911 |
| C++20 | cpp20-coroutine · 协程挂起、恢复与句柄所有权 | __cpp_impl_coroutine >= 201902; __cpp_lib_coroutine >= 201902 |
| C++23 | cpp23-expected · expected 与可解释错误 | __cpp_lib_expected >= 202202 |
| C++23 | cpp23-if-consteval · if consteval 与求值语境 | __cpp_if_consteval >= 202106 |
| C++23 | cpp23-byteswap · byteswap 与字节序 | __cpp_lib_byteswap >= 202110 |
| C++23 | cpp23-contains · contains 与查找谓词 | __cpp_lib_string_contains >= 202011 |
| C++23 | cpp23-underlying · to_underlying 与枚举边界 | __cpp_lib_to_underlying >= 202102 |

固定负例必须观察到特定失败阶段、非零退出码和实际诊断，修复必须编译、实际运行、输出 42 并退出 0；工具链/特性检查必须先成功。源代码与命令固定，复用临时目录、同源 CSRF、并发锁、输出和进程时限；不接受任意命令/源/路径/参数，不改变正式课程历史。所有运行期负例为定义良好的观察差异或受控异常；协程句柄所有权明确，线程停止握手有界于 runner。网页支持查阅、真实重放、课程关联、独立迁移练习说明；尚不是可编辑现代正式工程课程。

## 验收

- 独立 CI job Ubuntu 24.04 安装 g++-13，实测全部 10 对/20 变体，记录版本、命令、阶段、退出码、实际输出；真实 concepts 与 expected 修复源在 C++17 模式被拒绝。
- 检查现代编译器不可用时不把失败标为已观察，同时经典 C++17 实验仍用原 g++ 正常运行；API 不接受额外 compiler 参数。
- Chromium 在原工作台实测 concepts / expected 两对并核对课程记录不变，保留所有已有浏览器/正式工程回归。
- 未完成 CI 前为候选；后续需补现代独立可编辑课程与实际学员评估。只覆盖表列代表性特性，不宣称完整 ISO 标准或真人掌握。
