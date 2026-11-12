# CppAppLab

**CppAppLab** is a hands-on C++ learning and engineering-practice platform. It takes a learner from the first C++ program to maintainable Linux systems software, concurrent programs, TCP services, and an HTTP/1.1 server.

## 开始学习：先看课程，再做练习

CppAppLab 当前发布了 **23 个可操作的 W1–W9 学习任务**。不必先研究环境安装步骤；已有支持环境的学习者可以直接进入课程：

1. 在源码项目根目录运行 `./build/cppapplab`；如果使用学习者压缩包，在解压目录运行 `./cppapplab`。
2. 选择“继续学习”自动开始下一关；输入 `guide` 或在首页选择 **5** 可阅读[中文学习导航](course/LEARNING_GUIDE_ZH.md)。
3. 只修改任务列出的工作区文件；遇到问题按提示逐级求助，最后通过固定验收再继续下一关。

当前发布范围为 W1–W9 已实现任务；W9-A4/A5 及 W10–W12 尚处于路线图中。环境要求与部署说明保留在后文，供确有需要时查询。

## First-version baseline

- **Host:** Windows 11 with WSL2
- **Linux:** Ubuntu 24.04 LTS
- **Language:** C++17
- **Compiler:** GCC / g++
- **Build:** CMake
- **Debugger:** GDB
- **Version control:** Git and GitHub
- **Tests:** GoogleTest and CTest
- **Quality:** compiler warnings, clang-format, clang-tidy, ASan and UBSan
- **CI:** GitHub Actions on Ubuntu

Linux/POSIX topics use real Linux APIs. The first release does not claim full native MSVC parity; Windows-native compatibility can be added for portable course modules after the Linux baseline is stable.

## Current learning tasks

The main branch contains 23 hands-on tasks across W1–W9. W1–W7 cover C++ fundamentals, Linux systems, concurrency and TCP; W8 builds the HTTP server and its six D6 engineering tasks; W9 adds chunked transfer decoding, strict request-target/query parsing and descriptor-confined static-file access. `course/tasks.tsv` is the authoritative task catalog.

**推荐先读：** [中文学习导航（23 个任务）](course/LEARNING_GUIDE_ZH.md)；也可以在终端运行 `./build/cppapplab guide`。重点是做题、理解报错与通过固定验收，不必先阅读安装文档。

**Recommended learner entry point — interactive home:**

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

The detailed course map and acceptance boundaries live in [course/CURRICULUM.md](course/CURRICULUM.md). W2's Todo task exercises STL algorithms and durable file I/O through a cumulative command-line application.

## Project principles

1. **Real programs before isolated syntax drills.** Each concept is introduced in the context of a working program.
2. **Reference and learner work are separate.** Reference solutions are not used as learner submissions.
3. **Fixed acceptance contracts.** Platform-owned tests validate behavior; learner tests and build files are not the authority for acceptance.
4. **Hints do not replace implementation.** Learners can request progressive hints, implement the task, and rerun checks.
5. **Evidence is reproducible.** A check records compiler output, toolchain metadata, source fingerprint, and exit status.
6. **No fake portability.** POSIX and Linux networking lessons use the actual Linux APIs they teach.
7. **CI is not human learning validation.** Green automation proves build/test behavior, not that a learner understands the code.
8. **Progress survives sessions.** Workspace locations and check history are stored outside the source checkout; current PASS state is revalidated against the workspace source fingerprint.

## Repository status

W1–W7 and W8-D1–D6-D3 are merged into `main`. The HTTP reference server includes sequential Keep-Alive, connection deadlines and quotas, graceful shutdown, structured JSONL logs, runtime metrics, and operational load/soak regression coverage. PR #25 merged as `597a8a0`; final-head CI run `42991542424` passed all three jobs. The six D6 learner tasks were merged via PR #26 (`c8f8e03`). Final PR-head CI run `42998617546` passed GCC, ASan/UBSan and clang-tidy. The 49 task-mode acceptance checks passed in all three configurations, and the root learning-engine suite passed 60/60. Windows 11 + WSL2 target acceptance, offline dependency packaging, and release qualification remain separate. W9-A1 (`w9.http-chunked`) was merged via PR #27 (`bfb3f3f`). Final PR-head CI run `43006252261` passed all three jobs: root 62/62 and W9 fixed fixture 35/35 under GCC, ASan/UBSan and clang-tidy. The real learning-engine workflow also created the W9 workspace, invoked `cppapplab check` and verified current PASS evidence. W9-A2 (`w9.request-target`) was merged via PR #28 (`cca1c80`). Final PR-head CI run `43008862704` passed all three jobs: root 64/64, W9-A1 35/35 and W9-A2 39/39 under GCC, ASan/UBSan and clang-tidy. Both W9 learner workspaces passed the real `cppapplab check` workflow. W9-A3 (`w9.static-root`) merged via PR #29 (`f741e34`). Final PR-head CI run `43013017702` passed all three jobs: root 66/66 and W9-A3 34/34 under GCC, ASan/UBSan and clang-tidy. W9-A1 remained 35/35 and W9-A2 39/39; the real `cppapplab check` workflow verified current PASS evidence for A3. The learner usability improvements are now merged through PRs #30–#32: guided task progression, an interactive home, actionable failure diagnostics, title-keyword task search and granular progress states. Final PR #32 CI run `43024884781` passed root GCC/CTest 75/75, root ASan/UBSan 75/75, clang-format, clang-tidy and the configured W8/W9 fixture gates. PR #33 (986e9ff) adds a Windows double-click launcher for WSL2 Ubuntu 24.04; final CI run 43031051999 passed GCC 75/75, ASan/UBSan 75/75, clang-format/clang-tidy, Windows PowerShell 5.1/7 syntax checks and Bash bootstrap path/checkout-safety tests. Actual WSL2 target-machine acceptance remains separate. See:
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

P7-A source and learner package candidates are merged and verified in [CI run #1183](https://github.com/clonetimer/CppAppLab/actions/runs/43132738098). P7-B is building the offline Ubuntu 24.04 dependency bundle with read-only APT URI resolution and local-only installation; routine CI does not install Ubuntu packages. See [release packaging](docs/RELEASE_PACKAGING.md) and [project status](docs/STATUS.md).

## Quick start

After cloning the repository inside Ubuntu 24.04 (WSL2 is the target environment), run one command from the repository root:

```bash
bash scripts/quickstart.sh
```

The script checks the environment, offers to install missing Ubuntu build dependencies, configures and builds the project, runs the platform tests, and opens the interactive learning home. Windows users can also double-click `Start-CppAppLab.cmd` from a Windows-accessible checkout (including a checkout opened through the WSL share); it detects WSL2 Ubuntu 24.04, clones the private checkout into WSL on first launch when needed, and opens the learner home. It does not overwrite existing learner workspaces. After setup, run `./build/cppapplab` for the interactive home or `./build/cppapplab learn` to continue directly. The task picker accepts a number, task ID or title keyword. Failed checks now show a short diagnostic excerpt and the next hint, log and retry commands. For a manual setup path, see [Environment setup](docs/ENVIRONMENT_WSL2_UBUNTU.md).

After satisfying the static-files prerequisite, start `w8.router` like any other learning task. For a reference-only demonstration:

```bash
cmake -S labs/w8/router/reference -B build-router
cmake --build build-router --parallel
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' | ./build-router/http-router parse-stdin
```

The Router CLI is a one-request application-layer demonstration. The W8-D5 server target binds a real TCP port; run `./build/http-server server 8080 www` to start it.

## Scope boundary

CppAppLab is an educational engineering lab, not a production C++ framework or a substitute for official compiler, CMake, Linux, or protocol documentation. Course acceptance focuses on correctness, resource lifetime, error handling, reproducible builds, tests, and explainable design decisions.
