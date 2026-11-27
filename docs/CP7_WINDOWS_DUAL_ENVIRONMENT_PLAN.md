# CP7 — Windows 原生教学与双环境统一

日期：2026-11-27。基线：CP6-B PR #84 已合并 `731522a4eeab44dbb05114f87efcae7d2d3ffd8b`，CI 45947613724 的 14 个适用 jobs PASS。上一轮 Ubuntu 学习者包与网页服务保留。本轮实施方向由用户明确启动：Windows 11 x64 原生 W1–W4，现代 C++ 代表性教学；Linux 专项保留 WSL2。真实目标机器和真人学习评估转后期反馈，仍须标记 NOT RUN。

## 冻结范围

- `course/tasks.tsv` 是唯一正式任务目录，25 行、原 prerequisites、源码边界与验收合同保持不变；下表为静态审计快照，绝不是运行时能力宣称。
- 首批 W1–W4 五任务：Hello World 原输出检查，Calculator 9、Todo 19、TaskManager 31、Quality Gate 41 项（100 CTest 加一项精确输出合同）。W5–W9 全部沿用 Ubuntu；W6 依赖 W5，即使代码可移植也不跳过前置。
- C++17 主干不升级；CP4 已有 10 对 C++20/23 探针、2 个独立项目 7+8 测试另行验证。W10 #82 不纳入当前主线。
- 不再采用旧 ADR-0007 的全课程 Windows-only 方向。#67 的旧分支及历史审计保留，不整体合并；选择性迁入与当前目标一致的已验证代码。
- Windows 原生支持仅能在真实根引擎、工作区、工具链、课程和网页交付全链通过后声明。当前产品仍只支持 Ubuntu 22.04/24.04，Windows 用户走 WSL2；隔离测试的 PE 可执行文件不等于产品已原生。

## 课程环境审计

| 任务 | Windows 路径目标 | 既有 Linux 路径 | 当前原生状态 |
|---|---|---|---|
| `w1.hello-world` | 原生首批候选 | Ubuntu 22.04 / 24.04 | NOT QUALIFIED |
| `w1.calculator` | 原生首批候选 | Ubuntu 22.04 / 24.04 | NOT QUALIFIED |
| `w2.todo-cli` | 原生首批候选 | Ubuntu 22.04 / 24.04 | NOT QUALIFIED |
| `w3.task-manager` | 原生首批候选 | Ubuntu 22.04 / 24.04 | NOT QUALIFIED |
| `w4.quality-gate` | 原生首批候选 | Ubuntu 22.04 / 24.04 | NOT QUALIFIED |
| `w5.system-lab` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w6.thread-pool` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w7.tcp-echo` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w7.tcp-chat` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-protocol` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-parser` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.static-files` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.router` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-server` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-connection-policy` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-keep-alive` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-connection-guards` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-graceful-shutdown` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-observability` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w8.http-operational-load` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w9.http-chunked` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w9.request-target` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w9.static-root` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w9.http-hardening` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |
| `w9.http-fuzzing` | Linux / WSL2 专项 | Ubuntu 22.04 / 24.04 | 保留现有 Linux 验收 |

## 实施与验收

| 阶段 | 交付 | 必须满足 |
|---|---|---|
| CP7-A | 范围、历史差异、架构和依赖审计 | 本文件及 ADR-0009；保留全部 25 行与 Linux CI |
| CP7-B1 | 复用 Win32 进程后端并在最新主干原生执行 W1 早期夹具 | Windows 2025 UCRT GCC target、PE 检查、共用进程契约、W1 starter 失败 / 修复等价文本检查、Calculator 9/9；原 Linux 矩阵完整通过 |
| CP7-B2 | 持久化、路径、工具链与根学习引擎 | Windows 原生文件锁、原子发布、UTF-8/宽路径、重解析点/并发安全、固定 CMake/GCC 配置；学习命令受平台能力约束 |
| CP7-C | W1–W4 原生课程 | 全部 100 项固定 CTest + 精确输出；真实 CLI/网页 starter FAIL / 修复 PASS / stale / resume / prerequisites；不跳过安全测试 |
| CP7-D | 原生免安装交付 | 单独 Windows x64 ZIP，固定工具链版本、哈希、许可证、离线运行；PowerShell/cmd 启动，无 Bash/WSL/apt 产品依赖 |
| CP7-E | 现代 C++ | CP4 原有 10 对实验和两项目 15 项固定测试；按实际编译器特性验证，不扩大 ISO 覆盖声明 |
| CP7-F | 双环境统一 | 原生默认、Linux 按需；不安装 WSL 也能学习 W1–W4；执行端能力、工作区及验收来源分离；不得无证据迁移 PASS |
| CP7-G | 软件综合收口 | 全适用 CI、对应交付包实际解压与真实编译、离线、非管理员、中文/空格路径；实机与真人反馈后补 |

## CP7-A/B1：历史复用和当前实现

#67 的旧 head `08e3666cca90e79295a021f3cc1f2b965bcee518`，CI [44561161976](https://github.com/clonetimer/CppAppLab/actions/runs/44561161976) 曾通过原生进程专项和该时点的 Linux 回归。与当前 main 比较，旧分支 ahead 16、behind 32，故仅选择性复用 `src/process_win32.cpp`、隔离 CMake 夹具、两个共享进程测试文件（仅增加引号/空参数断言、修正 Windows 子进程 argv[0]）。原生后端 CreateProcessW + Job Object、句柄白名单、二进制管道、超时、子孙清理，不替换 Linux 实现。旧 #67 留 Draft。

本候选新建独立 Windows CI，执行原生进程合同、真实 native GCC 编译/运行 Hello World starter 与固定测试源，以及原 Calculator 固定夹具 9/9，使用含空格路径。Windows 的 C++ 文本 stdout 默认 CRLF，W1 测试只将 CRLF 映射为 LF 后进行精确比较，拒绝裸 CR、缺失末尾换行、额外文本或非零退出。此映射仅用于新增 W1 文本对照测试，不能对通用二进制进程输出做全局换行转换。正式引擎尚未移植，未来必须在原生文本验收边界明确实现并验证相同规则。W1 测试不产生学员 PASS 或历史。

当前根 CMake 仍有 Linux-only guard，不绕过；`src/progress.cpp` 的 flock/open/fsync/O_NOFOLLOW、`src/learning.cpp` 的 POSIX 进程/可执行文件及路径、`src/workspace.cpp` 的重解析点安全，以及网页的 start_new_session/killpg 都是后续真实阻塞点。W2–W4 已有 CMake 直接 CLI 多进程测试，但文件 symlink 安全断言与 Windows 非管理员环境须单独验证；不得跳过。CI 安装 MSYS2 UCRT 工具链仅用于构建与验证原生 PE，不是未来产品的运行时依赖；用户 ZIP 工具链尚未交付。

首次候选状态：待当前 head 的 Windows/Ubuntu CI。禁止把旧 CI 或新增源文件当作当前版本原生验收通过。


## CP7-A/B1 verified; CP7-B2a native state integration (2026-11-27)

CP7-A/B1 merged as `16f072e7bfc4f064b0690e6ea8ecc85232f54f41` (#85).
Final [Windows run 45951955376](https://github.com/clonetimer/CppAppLab/actions/runs/45951955376)
used native UCRT64 GCC 16.2.0: shared process + W1 foundation 13/13 and original
Calculator fixture 9/9. [Existing CI 45951955373](https://github.com/clonetimer/CppAppLab/actions/runs/45951955373)
passed 14 applicable jobs, push-only compatibility skipped. These are isolated
native foundations, not the full Windows learner application.

B2a integrates the Win32 state adapter into the actual shared progress and workspace
sources, without changing the root Linux-only build guard. Native progress defaults
to `%LOCALAPPDATA%/CppAppLab/native-windows/progress`; explicit CPPAPPLAB_STATE_DIR
must designate a dedicated native state directory, not an imported Linux record
store. The existing progress/history format and source-fingerprint semantics are
retained; workspace paths round-trip as UTF-8. Existing Linux I/O uses its
original POSIX implementation. Native progress and workspace evidence use wide
paths, no-follow file handles, checked local fixed-drive ancestors, bounded
reads, CREATE_NEW temporaries, FlushFileBuffers and same-volume MoveFileExW replacement after the temporary is closed; checked parent directory handles stay pinned without FILE_SHARE_DELETE.
History append and read use bounded LockFileEx acquisition, with the original
1 MiB history limit. Existing reparse-point ancestors, reparse files, hard-linked
files and alternate streams are rejected. Directory handles are held without
delete sharing during each operation. This protects normal local application
state; it is not an arbitrary same-user adversarial sandbox, a multi-record
transaction, or a power-loss durability guarantee. Full workspace creation/copy
path confinement remains a separate gate before the native root engine is exposed.

The Windows workflow adds seven native state tests, including actual shared
ProgressRecord/workspace/evidence APIs, Chinese and space paths, stale
fingerprint/prerequisite transitions, real NTFS junction and hard-link
rejection, bounded append, and four separate native child processes appending
80 uniquely identified rows to one history. Test evidence used to exercise
storage is fixture-only, not a learner's compiler PASS. No fixture may silently
skip a junction/hard-link assertion. The workflow checks 20 native process/W1/state
tests (13 previous + 7 new), all original 9 Calculator cases, PE/import identity,
and the unchanged original Ubuntu workflow. This B2a revision is a candidate
until current CI completes. The next gate is native safe workspace creation,
fixed native toolchain/profile and full root learning engine; no Windows ZIP or
native browser product claimed yet.


### CP7-B2a regression findings and correction (2026-11-27)

The first native state run failed four cases because the initial absolute-path rename returned success without the expected destination becoming visible. The second attempt used a non-null RootDirectory with a relative leaf and was rejected by Windows with ERROR_INVALID_PARAMETER. The final implementation uses the documented Win32 same-volume MoveFileExW path form. It holds checked parent-directory handles (without FILE_SHARE_DELETE), writes and flushes an exclusive CREATE_NEW temporary, closes it before MoveFileExW (the documented API requirement), and invokes MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH. MOVEFILE_COPY_ALLOWED is deliberately absent: source and destination are siblings on the same fixed volume; any cross-volume fallback would weaken atomic replacement. MOVEFILE_COPY_ALLOWED is deliberately absent: source and destination are siblings on the same fixed volume; any cross-volume fallback would violate atomic replacement. This implementation passed the final current-head Windows and Linux CI runs recorded below. The previous failures remain recorded and are not counted as passes.


### CP7-B2a final verification (2026-11-27)

Candidate head `75990c2d2de4fec208fae9a2d469d45b693c5fb0`:
- Windows run [45957192856](https://github.com/clonetimer/CppAppLab/actions/runs/45957192856): 20/20 process/W1/native-state tests and original Calculator 9/9 passed. The final tested implementation flushes and closes the temporary before MoveFileExW.
- Existing Linux regression [45957192865](https://github.com/clonetimer/CppAppLab/actions/runs/45957192865): all 14 applicable jobs PASS; one push-only compatibility check skipped as configured. This includes both Ubuntu builds, ASan/UBSan, clang-tidy, Chromium, CP4 GCC13, CP5 fuzz, package and offline-dependency gates.
- PR #86 is ready for merge. B2a qualifies native progress/workspace evidence storage and the W1/Calculator native fixtures only. It does not qualify native workspace creation/copy, full learning engine, Python browser supervision, a Windows package or target-machine acceptance.


## CP7-B2b — Native workspace creation and safe template copy (candidate)

Implemented on top of CP7-B2a main `a6f1aab087a7da902d73ed794398203dcb04e01a`:

- Windows `create_workspace` now rejects existing destinations, creates parents through the checked native directory API, creates the workspace exclusively, copies the actual learner template without following reparse points, publishes the workspace marker, and cleans partial workspaces through no-follow removal.
- `open_workspace` validates the entire directory chain with native handles. Source fingerprints, evidence and marker reads already use the shared native I/O adapter.
- Task catalog loading now reads the real `tasks.tsv` using wide-path no-follow I/O; `CPPAPPLAB_DATA_DIR` is read with `GetEnvironmentVariableW`, and compiled data roots use UTF-8 path decoding. This allows a Unicode install/data directory.
- Tree copy rejects reparse-point entries, device entries and multiply linked files. Source and destination trees may not contain one another. Cleanup removes a final junction/reparse entry itself rather than traversing its target.
- Five new tests cover workspace creation/reopen in Unicode paths, template junction rejection, hard-linked template rejection, destination-parent junction rejection, and safe junction cleanup. The Windows workflow count is raised from 20 to 25 tests (13 process/W1 + 7 state + 5 workspace); original Calculator remains 9/9. No course row or fixture contract changed.

Current status: implementation candidate; fresh Windows and Ubuntu CI pending. Root CMake still rejects Windows, the complete `learning.cpp` CLI and Python browser supervisor are not yet native, and there is no Windows package. The next B2c gate is to qualify the native workspace/security tests and then port toolchain/process/environment/CRLF behavior in the learning engine without exposing Linux-only W5–W9 on Windows.


## CP7-B2c — Root Windows-native learning engine (candidate, PR #88)

**Goal:** build the existing shared C++ learning engine as a real `cppapplab.exe` on Windows, rather than treating the isolated process/state fixtures as product completion.

### Implementation in candidate
- Root CMake now selects the existing Win32 `CreateProcessW` process backend and no-follow Windows state adapter on Windows; Linux retains its original POSIX process backend.
- Native Windows CMake is explicitly qualified only for MinGW-w64 UCRT GCC. MSVC is not claimed.
- The Windows entry point is `wmain`; arguments are converted from UTF-16 to UTF-8. The shared CLI converts filesystem arguments from UTF-8 back to native Windows paths.
- The shared CLI now uses `USERPROFILE`, `GetEnvironmentVariableW` / `SetEnvironmentVariableW` for native path environment, Windows console detection, `gmtime_s`, `.exe` learner outputs, and `g++.exe` / `cmake.exe` / `ctest.exe`.
- Course material reads and declared C++ source staging use native no-follow file operations. The fixed stdout contract remains byte-exact except for normalizing only the Windows CRT's valid LF-to-CRLF text-mode conversion; all other bytes and exit/timeout/output constraints remain strict.
- Five new native integration tests cover Unicode USERPROFILE workspace creation/resume, actual W1 compile/check/evidence/history/unlock, wrong-output rejection and prerequisite lock, and Unicode lesson/hint reads.
- The primary `CppAppLab CI` Windows job now installs UCRT64 and runs the 25 process/state/workspace tests, original Calculator 9/9, root CMake build, four learning integration tests, native PE import checks and a real Unicode `cppapplab.exe start → lesson → check → history → learn` journey.

### Acceptance boundary
PR #88 is still a candidate. Do not claim native learner completion until the Windows job has actually built and run the root project and all five integration tests, and the existing Ubuntu 22.04/24.04 matrix is green. A source-only or isolated process test is insufficient.

**Still not delivered:** Windows learner package/installer, Windows browser/Python supervisor, MSVC support, W5–W9 native execution, and real target-machine acceptance. W5–W9 remain on Linux/WSL2; all 25 tasks and fixed acceptance contracts remain unchanged.
