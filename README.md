# CppAppLab

> **当前主线（2026-11-27）：CP7 Windows 原生教学与双环境统一。** 目标为 Windows 11 x64 原生 W1–W4、现有现代 C++ 教学和保留完整 Ubuntu/WSL2 Linux 专项。W1–W4 的共享 Windows 原生 CLI 已通过 101 项固定验收（#93，含中文工作区；实际用户机器及符号链接权限尚待确认）。现有正式学习者包仍为 Ubuntu 22.04/24.04（Windows 可通过 WSL2）；原生网页候选尚待 CI，Windows 免安装学习者包尚未交付。参见[CP7 计划](docs/CP7_WINDOWS_DUAL_ENVIRONMENT_PLAN.md)及[ADR](docs/DECISIONS.md)。

**CppAppLab** is a hands-on C++ learning and engineering-practice platform. It takes a learner from the first C++ program to maintainable systems software, concurrent programs, TCP services, and an HTTP/1.1 server.

## 开始学习：先看课程，再做练习

CppAppLab 课程目录包含 **25 个 W1–W9 学习任务**（W9-A1–A5 均已合并；A5 已完成有界 fuzz 验证）。

### 已构建源码项目：打开网页工作台

在 Ubuntu 的项目根目录运行下面的命令。

```bash
bash scripts/start-web-workbench.sh
```

用浏览器打开 [http://127.0.0.1:8765/](http://127.0.0.1:8765/)。第一次学习可以先点 **第一次使用？先看四步说明**，关闭指南后点 **继续学习**，再点 **开始 / 继续学习** 打开第一关的工作区。按课程要求修改 `src/main.cpp`，依次点击 **构建**、**运行程序** 和 **正式验收**。验收通过后，结果显示 **PASS**。之后可从顶栏随时打开 **中文学习指南**。

**知识导航：** 课程目录旁「查知识」支持中英文术语搜索，关联现有讲义和固定独立失败/修复实验。真实编译诊断可进入对应知识点；实验不影响正式课程验收或源码。生命周期与并发先阅读已有课程，不执行故意未定义行为。

**教程语言：**“课程讲解”标题旁的 **教程语言** 可以选择 **中文 / English**。首次打开默认中文，浏览器会记住选择；课程目录、正文、故障排查、验收要求和三级提示随之切换。全部 25 节课的中英文内容随项目提供，无需在线翻译。切换语言不会重载工作区或改变代码、运行输出与验收进度；代码示例、文件名和命令保留原样。此设置控制课程内容，操作界面仍使用中文。

如果源码目录还没有 `build/cppapplab`，先按[快速入门的构建步骤](docs/QUICKSTART.md#2-run-the-one-command-setup)完成构建，再启动工作台。

### Windows 原生网页：源码构建候选（尚待 CI）

在已有 MinGW-w64 UCRT GCC、CMake、Ninja、GoogleTest 和 Python 的 Windows 环境中，完成原生 CMake 构建后，可用 PowerShell 启动候选：

```powershell
.\scripts\start-native-web-workbench.ps1 -Binary .\build\cppapplab.exe
```

打开本机 `http://127.0.0.1:8765/`。本候选不调用 WSL、不自动安装工具或修改系统设置；W1–W4 可执行，W5–W9 教材仍可阅读但实际运行需 Ubuntu。Windows 原生知识实验及现代工程尚待验证。创建真实文件符号链接需开发者模式或相应权限，当前尚非双击免安装交付版。

### 使用学习者压缩包：直接打开网页工作台

在对应 Ubuntu 22.04 或 24.04（包括 WSL2）中解压学习者包，从解压目录执行：

```bash
./start-web-workbench
```

打开终端显示的本地 URL，保持终端运行；Ctrl+C 停止。学习者包内置预编译引擎、网页服务和静态页面，无需重新构建平台；练习本身仍需 GCC/CMake/GoogleTest，浏览器服务需要 Python 3。断网时保留基础编辑器，现代 GCC 13 工程仍受原工具链要求约束。上述能力须以本轮双 Ubuntu 打包回归验收为准。

### 使用学习者压缩包：打开命令行首页

在解压目录运行 `./cppapplab`，选择 **1. Continue learning**（继续学习）。源码构建也可用 `./build/cppapplab` 打开命令行首页。在首页选 **5** 或在终端给启动命令加上 `guide`，即可阅读[中文学习导航](course/LEARNING_GUIDE_ZH.md)。

当前发布范围为 W1–W9 已实现任务；W9-A4/A5 已合并并通过各自验收；W10–W12 仍为路线图内容，不属于当前交付主线。环境要求与部署说明保留在后文，供确有需要时查询。

## Current implementation baseline

- **Host:** Windows 11 with WSL2
- **Linux:** Ubuntu 22.04 LTS or Ubuntu 24.04 LTS
- **Language:** C++17
- **Compiler:** GCC / g++
- **Build:** CMake
- **Debugger:** GDB
- **Version control:** Git and GitHub
- **Tests:** GoogleTest and CTest
- **Quality:** compiler warnings, clang-format, clang-tidy, ASan and UBSan
- **CI:** Ubuntu build/test matrices; the Windows runner currently validates only WSL launcher syntax and handoff.

The values above describe the current supported source-build/CI environment. CP7 introduces a limited native-Windows teaching path; isolated native acceptance does not yet qualify the learning engine, browser package or a target machine. Preserve WSL/Ubuntu and all Linux-specific contracts.

## Current learning tasks

The course catalog contains 25 tasks across W1–W9, including the merged W9-A5 parser-invariant task. W1–W7 cover C++ fundamentals, Linux systems, concurrency and TCP; W8 builds the HTTP server and its six D6 engineering tasks; W9 adds chunked transfer decoding, strict request-target/query parsing and descriptor-confined static-file access. `course/tasks.tsv` is the authoritative task catalog.

**推荐先读：** [中文学习导航（25 个任务）](course/LEARNING_GUIDE_ZH.md)。零基础学习者先看其中的「第一次使用」四步说明和常用词解释；也可以在终端运行 `./build/cppapplab guide`。重点是先完成第一关，再逐步理解工具和原理，不必预先读完整套安装与工程文档。

**Interactive command-line home:**

```bash
./build/cppapplab
```

Choose **Continue learning**, browse tasks by number, or search by a task ID or words from its title. The task list distinguishes PASSED, IN PROGRESS, NEEDS WORK, STALE, AVAILABLE and LOCKED. In non-interactive terminals, the command prints help instead of waiting for input. To bypass the menu and start the next task directly:

```bash
./build/cppapplab learn
```

CppAppLab automatically selects the next unlocked task, creates a workspace under `~/cppapplab-work/`, displays the task guide and prints the exact edit, hint and check commands. Running it again resumes an incomplete task; after PASS it advances to the next unlocked task. To choose a task explicitly:

```bash
./build/cppapplab learn w1.calculator
```

For learners who prefer the lower-level workflow, the original commands remain available:

```bash
./build/cppapplab list
./build/cppapplab progress
./build/cppapplab start w1.hello-world "$HOME/cppapplab-work/hello-world"
./build/cppapplab lesson "$HOME/cppapplab-work/hello-world"
./build/cppapplab hint "$HOME/cppapplab-work/hello-world" 1
./build/cppapplab check "$HOME/cppapplab-work/hello-world"
./build/cppapplab resume w1.hello-world
./build/cppapplab history w1.hello-world
```

## Course progress and workspace recovery

- `cppapplab progress` shows task states and prerequisite locks.
- `cppapplab resume <task-id>` reopens the most recently started workspace for that task, including before its first check.
- `cppapplab history <task-id>` shows previous acceptance attempts.
- A dependent task can start after its prerequisite has a current PASS in any saved workspace.
- A PASS is current only while its workspace exists and the declared source fingerprint still matches.

Progress is stored outside the repository by default:

```text
~/.local/share/cppapplab/progress/
```

Set `CPPAPPLAB_STATE_DIR` to move this state directory. The course catalog is maintained separately in `course/tasks.tsv`; it declares lesson paths, source files, acceptance type, and prerequisite relationships.

## Learning path

The current learning path intentionally teaches Linux/POSIX APIs for the systems and networking exercises. Future independent modern C++ labs must preserve the C++17 baseline and formal acceptance contracts.

| Stage | Focus | Main work product |
|---|---|---|
| W1 | Environment, C++ basics, multi-file compilation, CMake, GDB | Calculator |
| W2 | STL, algorithms, lambdas, file I/O | Persistent Todo CLI |
| W3 | Classes, RAII, smart pointers, copy/move semantics | Refactored Task / TaskManager |
| W4 | GoogleTest, debugging, sanitizers, static analysis, CI | Tested and versioned C++ project |
| W5 | Linux file APIs, processes, threads, synchronization | Thread-safe queue and producer/consumer demo |
| W6 | Thread pools, futures, templates, atomics, benchmarking | Reusable thread pool |
| W7 | TCP/IP, sockets, nonblocking I/O, select/poll/epoll | Multi-client TCP chat |
| W8 | HTTP/1.1, parser, response, routing, static files, event loop | Educational HTTP server |
| W9 | Chunked transfer, framing safety, URL and filesystem security, parser fuzzing | Hardened HTTP protocol stack |

The detailed course map and acceptance boundaries live in [course/CURRICULUM.md](course/CURRICULUM.md). The original 23 task rows and acceptance contracts remain frozen; CP5 may append a new task only with fixed learner boundaries and actual acceptance. W2's Todo task exercises STL algorithms and durable file I/O through a cumulative command-line application.

## Project principles

1. **Real programs before isolated syntax drills.** Each concept is introduced in the context of a working program.
2. **Reference and learner work are separate.** Reference solutions are not used as learner submissions.
3. **Fixed acceptance contracts.** Platform-owned tests validate behavior; learner tests and build files are not the authority for acceptance.
4. **Hints do not replace implementation.** Learners can request progressive hints, implement the task, and rerun checks.
5. **Evidence is reproducible.** A check records compiler output, toolchain metadata, source fingerprint, and exit status.
6. **No fake portability.** Linux-specific lessons use real Linux APIs. Native Windows basics must use real Win32 process and filesystem semantics and native GCC, not Bash/WSL compatibility layers or mocks; W5–W9 remain Linux lessons in CP7.
7. **CI is not human learning validation.** Green automation proves build/test behavior, not that a learner understands the code.
8. **Progress survives sessions.** Workspace locations and check history are stored outside the source checkout; current PASS state is revalidated against the workspace source fingerprint.

## Repository status

W1–W7 and W8-D1–D6-D3 are merged into `main`. The HTTP reference server includes sequential Keep-Alive, connection deadlines and quotas, graceful shutdown, structured JSONL logs, runtime metrics, and operational load/soak regression coverage. PR #25 merged as `597a8a0`; final-head CI run `42991542424` passed all three jobs. The six D6 learner tasks were merged via PR #26 (`c8f8e03`). Final PR-head CI run `42998617546` passed GCC, ASan/UBSan and clang-tidy. The 49 task-mode acceptance checks passed in all three configurations, and the root learning-engine suite passed 60/60. Legacy Windows 11 + WSL2 target acceptance, offline dependency packaging, and release qualification were tracked separately; WSL2 acceptance does not satisfy the new native-Windows target. W9-A1 (`w9.http-chunked`) was merged via PR #27 (`bfb3f3f`). Final PR-head CI run `43006252261` passed all three jobs: root 62/62 and W9 fixed fixture 35/35 under GCC, ASan/UBSan and clang-tidy. The real learning-engine workflow also created the W9 workspace, invoked `cppapplab check` and verified current PASS evidence. W9-A2 (`w9.request-target`) was merged via PR #28 (`cca1c80`). Final PR-head CI run `43008862704` passed all three jobs: root 64/64, W9-A1 35/35 and W9-A2 39/39 under GCC, ASan/UBSan and clang-tidy. Both W9 learner workspaces passed the real `cppapplab check` workflow. W9-A3 (`w9.static-root`) merged via PR #29 (`f741e34`). Final PR-head CI run `43013017702` passed all three jobs: root 66/66 and W9-A3 34/34 under GCC, ASan/UBSan and clang-tidy. W9-A1 remained 35/35 and W9-A2 39/39; the real `cppapplab check` workflow verified current PASS evidence for A3. The learner usability improvements are now merged through PRs #30–#32: guided task progression, an interactive home, actionable failure diagnostics, title-keyword task search and granular progress states. Final PR #32 CI run `43024884781` passed root GCC/CTest 75/75, root ASan/UBSan 75/75, clang-format, clang-tidy and the configured W8/W9 fixture gates. PR #33 (986e9ff) adds a Windows double-click launcher for WSL2 Ubuntu 24.04; final CI run 43031051999 passed GCC 75/75, ASan/UBSan 75/75, clang-format/clang-tidy, Windows PowerShell 5.1/7 syntax checks and Bash bootstrap path/checkout-safety tests. That WSL2 launcher remains the current Windows entry and does not constitute native Windows acceptance. See:
- [Architecture](docs/ARCHITECTURE.md)
- [Learning engine design](docs/LEARNING_ENGINE_DESIGN.md)
- [Roadmap](docs/ROADMAP.md)
- [Development and acceptance](docs/DEVELOPMENT.md)
- [Environment setup](docs/ENVIRONMENT_WSL2_UBUNTU.md)
- [First-use quick start](docs/QUICKSTART.md)
- [Current project status](docs/STATUS.md)
- [Router lesson and exact task boundary](course/lessons/w8/router.md)
- [HTTP TCP server lesson](course/lessons/w8/http-server.md)
- [HTTP connection policy lesson](course/lessons/w8/connection-policy.md)
- [HTTP Keep-Alive lifecycle lesson](course/lessons/w8/keep-alive.md)
- [HTTP connection guards lesson](course/lessons/w8/connection-guards.md)
- [HTTP graceful shutdown lesson](course/lessons/w8/graceful-shutdown.md)
- [HTTP observability lesson](course/lessons/w8/observability.md)
- [HTTP operational load lesson](course/lessons/w8/operational-load.md)
- [W9–W12 advanced curriculum overview](course/lessons/w9/overview.md)
- [W9-A1 chunked transfer lesson](course/lessons/w9/http-chunked.md)
- [W9-A2 request-target lesson](course/lessons/w9/request-target.md)
- [W9-A3 secure static-root lesson](course/lessons/w9/static-root.md)

## Delivery status

P7-A source and learner package candidates are merged. The Ubuntu 22.04 compatibility follow-up (PR #38) builds native learner packages for both Ubuntu 22.04 and 24.04; it also extends offline dependency bundles to matching Jammy/Noble targets with read-only APT URI resolution and local-only installation. See [release packaging](docs/RELEASE_PACKAGING.md) and [project status](docs/STATUS.md).

## Quick start

After cloning the repository inside Ubuntu 22.04 or Ubuntu 24.04 (WSL2 is the target environment), run one command from the repository root:

```bash
bash scripts/quickstart.sh
```

The script checks the environment, offers to install missing Ubuntu build dependencies, configures and builds the project, runs the platform tests, and opens the interactive learning home. Windows users can also double-click `Start-CppAppLab.cmd` from a Windows-accessible checkout (including a checkout opened through the WSL share); it detects supported WSL2 Ubuntu 22.04/24.04 distributions, clones the private checkout into WSL on first launch when needed, and opens the browser workbench. Use `-Cli` for the terminal home or `-Setup` to rebuild before opening the browser. It does not overwrite existing learner workspaces. After setup, run `./build/cppapplab` for the interactive home or `./build/cppapplab learn` to continue directly. The task picker accepts a number, task ID or title keyword. Failed checks now show a short diagnostic excerpt and the next hint, log and retry commands. For a manual setup path, see [Environment setup](docs/ENVIRONMENT_WSL2_UBUNTU.md).

After satisfying the static-files prerequisite, start `w8.router` like any other learning task. For a reference-only demonstration:

```bash
cmake -S labs/w8/router/reference -B build-router
cmake --build build-router --parallel
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build-router/http-router parse-stdin
```

The Router CLI is a one-request application-layer demonstration. The W8-D5 server target binds a real TCP port; run `./build/http-server server 8080 www` to start it.

## Scope boundary

CppAppLab is an educational engineering lab, not a production C++ framework or a substitute for official compiler, CMake, Linux, or protocol documentation. Course acceptance focuses on correctness, resource lifetime, error handling, reproducible builds, tests, and explainable design decisions.


基础知识覆盖审计见 [审计报告](docs/CPP17_KNOWLEDGE_COVERAGE.md)（51 项 W1–W6 抽样已收口，28 组独立实验双 Ubuntu 已验证；正式任务数不变）。

### 可选现代 C++ 教学

经典正式工程仍用 C++17，不需要升级编译器。现代知识对照使用独立 GCC 13.x 配置，Ubuntu 24.04 可手动安装 g++-13；没有工具链时知识仍可阅读，运行按钮不可用，不回退。首批 10 组 C++20/23 实验已在 Ubuntu 24.04 的 GCC 13.3.0 实测通过，见 [现代配置](docs/MODERN_CPP_PROFILE.md)。

第二轮两个独立可编辑现代项目（采样窗口统计、定宽报文头解析）已通过 #79 实际验收，网页「查知识 → 现代工程实践」访问；需要额外 GCC 13、CMake 和 GoogleTest，经典 C++17 任务不受影响。见 [独立现代工程](docs/MODERN_CPP_PROJECTS.md)。

### CP5 · W9-A4 请求准入策略（已验收）

追加 `w9.http-hardening`，不改变原 23 个任务及其验收契约。复用已验证 A1 解析器，学习首条消息边界、明确资源预算和确定性拒绝策略；24 项固定 GoogleTest、双语教材与三级提示。此关为本地内存输入策略，不是已经部署的加固服务器；已通过 #80 的 13 项适用 CI，双 Ubuntu 的六种配置均 24/24。

### CP5 · W9-A5 解析器不变量与有界模糊测试（已合并）

第 25 个任务 `w9.http-fuzzing`：学员实现检查器，固定 A1/A4 解析链、21 项 GoogleTest、八个种子与独立真实 Clang/libFuzzer + ASan/UBSan 专项。PR #81 已合并；14 项适用 CI jobs PASS，双 Ubuntu 六种配置固定测试 21/21，实际有界 fuzz 运行 10,000 次且未发现问题。只处理本地内存输入，不是网络测试工具，也不是安全认证；W10–W12 不属于当前交付主线。
