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
- Five new native integration tests cover Unicode USERPROFILE workspace creation/resume, actual W1-W4 fixed acceptance/evidence/history/prerequisite chain, wrong-output rejection and prerequisite lock, and Unicode lesson/hint reads.
- The primary `CppAppLab CI` Windows job now installs UCRT64 and runs the 25 process/state/workspace tests, original Calculator 9/9, root CMake build, four learning integration tests, native PE import checks and a real Unicode `cppapplab.exe start → lesson → check → history → learn` journey.

### Acceptance boundary
PR #88 is still a candidate. Do not claim native learner completion until the Windows job has actually built and run the root project and all five integration tests, and the existing Ubuntu 22.04/24.04 matrix is green. A source-only or isolated process test is insufficient.

**Still not delivered:** Windows learner package/installer, Windows browser/Python supervisor, MSVC support, W5–W9 native execution, and real target-machine acceptance. W5–W9 remain on Linux/WSL2; all 25 tasks and fixed acceptance contracts remain unchanged.


## CP7-B2c — Unicode CMake build-cache correction (candidate)

PR #92's Windows run [45969984755](https://github.com/clonetimer/CppAppLab/actions/runs/45969984755)
built the real root `cppapplab.exe`, passed the 25 foundation tests and the
original Calculator 9/9 fixture, and passed four of five root learning tests.
Actual W1 acceptance from a Chinese workspace, history and prerequisite
unlock succeeded using relative compiler paths and an absolute UTF-16
process-launch path. The remaining W1–W4 chain stopped at Calculator's
GoogleTest discovery in a Unicode build directory; W2–W4 were not reached.

This candidate retains Unicode learner workspaces, but relocates native
CMake fixture builds to an owned local ASCII cache (default: system temp
directory / CppAppLab-native-build; override:
`CPPAPPLAB_NATIVE_BUILD_ROOT`). The engine derives a per-workspace cache
key and checks the full workspace/task ownership marker before reusing it.
A checked short-path alias is optional; if no ASCII cache path can be
established, the CLI reports how to choose a writable local cache, not
how to rename a Windows account. The learner's declared source files,
original fixture and original shared CLI workflow script are staged
through the native no-follow adapter. No reference answer is copied by
production code. Build/check prepare fresh native fixture trees, and run
requires the current source-fingerprint stamp. CMake uses Ninja/GCC.
The fixed CTest count must match an exact successful summary line
(legacy or new CTest format), with nonzero exit, timeout, truncation or
zero expected tests rejected. All 25 catalog rows and original fixtures
remain unchanged.

Native CLI explicitly marks W5–W9 as Linux/WSL2-required, separately
from prerequisite locks; start/learn/build/run/check/app-build/app-run
cannot execute these tasks natively. Six native root tests include the
actual Unicode W1–W4 course chain, build/run versus formal PASS separation,
wrong-output evidence, and Linux-only rejection. The original W2–W4
symlink tests must run without skips.

Current status: candidate, fresh Windows/Ubuntu CI required. The cache
is ordinary local application storage, not a same-user security sandbox;
same-workspace simultaneous execution is not qualified. Native browser,
installer, offline tools, real nonadministrator target machine and
human learning evaluation remain unqualified.


### CP7-B2c/C — Windows file-symlink compatibility follow-up

Initial PR #93 head `498c380b`, native run [45974884979](https://github.com/clonetimer/CppAppLab/actions/runs/45974884979) and full run [45974885077](https://github.com/clonetimer/CppAppLab/actions/runs/45974885077): root native PE and 25/25 foundation tests pass; 5/6 native learning integration tests pass; real Hello World and Calculator (9/9) pass from a Unicode learner workspace. W2 reaches all 19 tests: 17 pass, two fail before invoking the learner code because MinGW's std::filesystem::create_symlink reports ENOSYS. W3/W4 not reached. All 13 non-Windows jobs pass; native Windows fails, one push-only job skipped.

Correct the **setup only** of the two original security tests in each W2/W3/W4 fixture. On Windows call CreateSymbolicLinkW and verify IO_REPARSE_TAG_SYMLINK, not a regular-file substitute. Retain Linux std::filesystem::create_symlink and all original names, counts, assertions, real CLI persistence workflows. No skip; absence of symlink creation permission remains an explicit failure. First correction run 45980630027 reached native root execution but failed compiling the fixture because MinGW already defines NOMINMAX=1. Guard both Windows header macros without relaxing -Werror.

GCC upstream symlink support work (https://gcc.gnu.org/pipermail/gcc-patches/2026-June/721803.html) also documents the Windows lstat-following-link limitation in the existing implementation. Provide a small `unsafe_data_link` helper in W2/W3 starter sources, native reference implementations and W4 prefilled implementation. Windows uses GetFileAttributesW to reject reparse points and unexpected inspection errors; Linux retains its standard symlink behavior. It is a leaf-path guard, not an arbitrary same-user sandbox. Preserve all original course algorithms and source declarations, and update the existing third hints without adding new hint levels. The W4 reference-test seeding path is corrected to reference/src/task_manager_test.cpp, matching the existing Linux workflow. Original 25 catalog entries and expected counts (1 exact output + 9+19+31+41) unchanged.

Delivery caveat: CreateSymbolicLinkW with SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE needs Developer Mode for non-elevated use (https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createsymboliclinkw). Hosted Windows CI is not evidence that a clean, nonadmin Windows 11 without this capability can perform W2-W4 formal checks. This is a real CP7-D/G gate; do not silently enable system settings or weaken the security contract.

Current candidate remains PR #93, pending fresh complete CI. No Windows browser/ZIP or target-machine/human claim.


## CP7-B2c/C verified; CP7-F1 native browser candidate

PR #93 merged at `3e2fdaeee664274c6b5286553edd73d938fbab23`.
Final Windows run [45981293387](https://github.com/clonetimer/CppAppLab/actions/runs/45981293387):
25/25 original native foundations, independent Calculator 9/9, six root
integration tests including the complete W1-W4 Unicode workspace chain,
and external native Unicode CLI smoke. The original formal contracts
(Hello World 1 + Calculator 9 + Todo 19 + TaskManager 31 + QualityGate 41)
all passed through the shared root engine. Full CI
[45981293216](https://github.com/clonetimer/CppAppLab/actions/runs/45981293216):
all 14 applicable jobs passed, one push-only job skipped. The hosted
Windows result qualifies the engine and fixed course acceptance, not
a clean nonadministrator Windows 11 with Developer Mode disabled.
Real user acceptance and learning efficacy remain NOT RUN.

F1 is deliberately a source-built native browser candidate ahead of
packaging: reuse the existing local Python HTTP service and UI, bind
loopback exclusively on Windows, supervise actual CLI processes, and
rely on the existing native engine's non-inherited kill-on-close Job
Objects for descendants. Track live native CLI calls, reject new calls
after shutdown, terminate active CLI roots on timeout or controlled
server shutdown; abrupt external termination of the Python supervisor
is not a qualified cleanup path. Native browser API actions are
serialized to avoid simultaneous edits/checks within this service.
Local native path checks reject reparse points, hard-linked regular
files, device names, alternate streams and unsafe ancestors before
resolution; this is not a same-user security sandbox. Preserve the
Linux browser/service path and fixed mutation token/origin guards.

The native UI shows all 25 tasks but marks 20 as Linux/WSL2 required,
keeps lessons and hints readable, limits execution to five W1-W4
tasks and counts only those five in native progress. Unqualified
knowledge experiments, modern C++ projects and network service
execution are explicitly unavailable on native Windows; no false
PASS or silent Linux fallback. No visual redesign.

Source-build entry point: `scripts/start-native-web-workbench.ps1`,
with an explicit native executable and Python on the caller's PATH;
no WSL invocation, toolchain download, global PATH changes or
Developer Mode changes. CI uses installed UCRT GCC and Python 3.12,
checks native HTTP W1 red/build/run/green, Calculator 9/9, restart,
history/stale behavior, hard-link and port guards, and actual
descendant shutdown. Pinned Chromium runs the native fallback-editor
DOM journey and captures Windows screenshots. New CI pending.
This does not deliver the install-free Windows package or the unified
WSL switching mechanism. CP7-D, E and remaining F/G are open.


## CP7-F1 verified; CP7-D1 portable Windows delivery candidate

#94 merged at `95a7e7162a40e9971d0becdcf1392faaae3452d3`.
Full CI [45986395723](https://github.com/clonetimer/CppAppLab/actions/runs/45986395723)
passed all 14 applicable jobs (one push-only compatibility check skipped).
The native Windows job executed real HTTP W1 build/run/check, Calculator 9/9,
history, stale, restart, hard-link, token, port conflict, actual learner
descendant shutdown and a genuine Chromium offline-editor journey.
Windows screenshot artifact 15191289510 was uploaded in that run.
Native foundation [45986395713](https://github.com/clonetimer/CppAppLab/actions/runs/45986395713)
also passed. CP7-F1 is qualified for a source-built native workbench;
it does not yet qualify install-free packaging.

D1 moves the install step to the build runner, not the learner.
A separate Windows 2025 job builds the native Release engine, acquires
the official Python 3.14.10 AMD64 embedded ZIP against its published
SHA-256, and packages the actual MSYS2 UCRT64 installed package inventory
for GCC/CMake/CTest/Ninja/GTest/GDB and native dependencies. Preserve
the prefix layout and original license files; copy only installed
UCRT64 package-owned paths, with safe in-prefix link materialization.
Record the exact installed package metadata, source commit, each
file's SHA-256/size and ZIP hash. MSYS2 acquisition is currently a
version-observed CI snapshot, not yet a locked repeatable acquisition
of every dependency; this must be closed for a formal release.

Package launch uses Start-CppAppLab.cmd and embedded Python, without
external Python, shell compatibility layer, administrator elevation,
global PATH changes, or runtime tool download. The package sets a
child-only PATH to the bundled toolchain and Windows system folders,
isolates other toolchain environment variables, and opens the local
browser only after binding its own service. CLI wrapper uses the
same package data root. Unicode learner paths stay independent of
toolchain paths. A non-ASCII toolchain location requires an existing
verified ASCII short alias or extraction to an ASCII path; the
launcher can use a writable package cache when the default temp
directory is incompatible. Real file-symlink capability is probed
without changing Windows settings; if absent, W2-W4 full acceptance
remains conditional, not waived.

The new test extracts the actual ZIP into a relocated directory
with spaces, verifies all listed file hashes, strips the host's
toolchain PATH, exercises the batch launcher and embedded Python,
opens the packaged HTTP service, performs real W1 red/build/run/green
and all original W1-W4 fixed checks (101 contracts) using actual
learner code, verifies relocated GCC/GTest paths in all four CMake
caches, then tests port conflict and restart/resume/stale. Test-only
reference seeding is not production behavior. Fresh CI pending.

D1 is an internal CI candidate, not a public release. The complete
corresponding third-party source and license/redistribution review,
immutable upstream dependency acquisition lock, nonadmin Windows 11
without file-symlink capability, fully offline physical machine,
Unicode install path without ASCII alias, CP7-E native modern projects
and knowledge experiments, CP7-F automatic environment handoff and
real learner feedback remain open.


### CP7-D1 actual archive and UTF-8 launcher correction

[Portable run 45990927948](https://github.com/clonetimer/CppAppLab/actions/runs/45990927948)
at head ce1ba4ee (tested merge ecf625d8ba4d) successfully built the strict
Release executable, acquired and verified embedded Python, collected the
installed native packages including the metadata-only cc-libs aggregator,
created the ZIP, and extracted/verified all 40,614 manifest files. Archive
SHA-256 in the log: 4ad4225c73dbdfdc59a77770ae6300bcdbc270ea3f0b7bfc3d03a1d31df9cac0.
The actual batch --verify-tools step failed before tool execution because
embedded Python's redirected stdout used CP1252 and the Chinese startup
message could not be encoded. No extracted learner-course check ran and no
verified artifact was uploaded. The full primary regression
[45990927981](https://github.com/clonetimer/CppAppLab/actions/runs/45990927981)
passed all 14 applicable jobs, including Ubuntu 24.04 sanitizer; foundation
45990927940 passed. Fix the launcher and smoke harness to explicitly
configure their own UTF-8 stdout/stderr, retaining the exact Chinese banner
assertion through the batch launcher and redirected output. Do not rely on
PYTHONIOENCODING in isolated embedded mode or change the system codepage.
New current-head CI is required before merge.


### CP7-D1 relocated GCC sysroot correction (candidate)

At head `6618a44e71af9dcdd71e56ba680e2d0d5b70edf1`, [portable run 45994789903](https://github.com/clonetimer/CppAppLab/actions/runs/45994789903) built the Release PE, verified the pinned embedded Python, created/extracted and checked 40,614 archive files (SHA-256 e6209b7d694c7feb140f57e307a0313358101d2d64d7a9aee63b922d8eb173c5), passed the Chinese batch banner, tool version checks and actual native HTTP startup. W1 formal red was observed, but the corrected W1 build failed in libstdc++ cwchar: `fatal error: wchar.h: No such file or directory`. No verified ZIP uploaded and no packaged course PASS established. The version probe alone was inadequate.

MSYS2 GCC 16.2.0-7's [build recipe](https://github.com/msys2/MINGW-packages/blob/master/mingw-w64-gcc/PKGBUILD) configures the sysroot to MINGW_PREFIX and native system headers to /include. The candidate explicitly supplies the verified relocated bundled prefix as CPPAPPLAB_NATIVE_SYSROOT; the shared native engine passes --sysroot to direct GCC builds/checks and CMAKE_SYSROOT to CMake fixtures. This is a targeted candidate correction, not a confirmed diagnosis of every GCC search path. Source-built native and Linux invocations without that package setting keep their prior behavior. Require actual wchar.h/windows.h bytes in the archive and a native compile/link/run probe during --verify-tools, not just --version. The smoke additionally requires W1 red to be the intended output mismatch and checks all four CMake sysroots against the relocated package. All 101 original fixed course checks, real symlink tests, compiler/GTest identity, restart/resume/stale, port conflict and current CI remain mandatory. No formal release or physical-machine PASS claimed until separately verified.


### CP7-D1 verified and merged; D2a acquisition capture candidate

PR #95 merged at `d9ee945c39261edd568e89af5b6a4f9fe8b98aa1`; final candidate head eadb38b7.
[Portable run 45996403950](https://github.com/clonetimer/CppAppLab/actions/runs/45996403950) verified 40,614 extracted files, actual bundled GCC compile/link/run, all five native courses (1+9+19+31+41 = 101/101), CMake sysroot/compiler/GTest identity, restart/resume/stale and port conflict. Tested PR merge b5f362179eafd1e8ec75728f127abe2e9a65c7e0, archive SHA-256 e2fd3e90d146450752f58ccea37f6a03ad68cf8679ef7fded07ee3813099f003; [internal artifact 15196891172](https://github.com/clonetimer/CppAppLab/actions/runs/45996403950/artifacts/15196891172), 395728938 bytes for outer Actions artifact. Foundation 45996403858 and 14 applicable primary jobs 45996404034 all PASS.

D2a freezes all 63 exact UCRT64 package versions from the successful run, including metadata-only cc-libs. Fail before building if the actual installed set has missing, added or changed versions. Capture exact package filenames/repository SHA-256 via pacman print format, then download the explicit exact targets without refreshing/installation and verify actual cached archive bytes. Preserve normal MSYS2 signature policy. Embed the baseline and acquisition report in the manifest-verified ZIP, check their identities in relocated acceptance and upload a small separate report only after all course acceptance. Portable unit tests reject drift, malformed records, missing and corrupt archive bytes. No package license or formal fixture changed.

This is D2a, not full immutable replay: setup-msys2 still performs the normal runner install and fails on upstream drift; the captured archive hashes must be reviewed and committed, then acquisition must replay the exact archives against those frozen hashes. Corresponding-source and redistribution approval, actual ordinary-user/offline Windows 11 and human learner feedback remain open. CP7-E and F unchanged. Fresh CI required.


### CP7-D2b frozen archive replay (candidate)

D2a portable [46000267714](https://github.com/clonetimer/CppAppLab/actions/runs/46000267714) passed the capture and complete relocated ZIP acceptance; small [report artifact 15197867836](https://github.com/clonetimer/CppAppLab/actions/runs/46000267714/artifacts/15197867836) was independently downloaded and matched its GitHub digest, inner JSON digest and checked-in version baseline. The complete 63 actual archive identities are frozen in `packaging/windows/ucrt64-archives.json` (SHA-256 `1118f65f6e4c0a3c0ec367785ffca648ddd629d38ec8fe03740f58ce3b13fd29`). D2b now requires exact archive size/hash plus trusted detached signatures, clean runner local-only transaction and replay receipt; package and smoke verify the binding. See `docs/CP7_WINDOWS_ARCHIVE_REPLAY.md`. Current D2b actual Windows replay and full CI pending; bootstrap environment, upstream availability, corresponding-source/redistribution and physical target remain open.


### CP7-D2b verified; D3a source/notice evidence candidate

#97 head 47e2a71c passed [portable 46001429435](https://github.com/clonetimer/CppAppLab/actions/runs/46001429435): 63 signed exact packages installed, 40,617 manifest file hashes, 101/101 native course contracts. Primary 46001429562 14/14, foundation 46001429630 PASS. Candidate artifact 15198846470, replay receipt 15197834438. D3a derives actual source identities from installed BASE/VERSION, downloads and checks signed archives and PKGBUILD listing, collects notice metadata and checks the official Python source SHA-256. Report has no public-release approval. See docs/CP7_WINDOWS_REDISTRIBUTION.md; project license choice, complete corresponding source, durable distribution, physical/human feedback remain open.


### CP7-E1 native fixed modern probes (candidate)

Reuse all 10 CP4 modern topics and the original 20 fixed sources/contracts. The verified portable profile selects the bundled GCC 16.2.0 and explicit sysroot; each run checks exact version, native target and dialect/feature macros. Linux GCC 13 profile unchanged. Browser accepts only existing topic/variant, invokes a fixed native engine step via the existing supervised CLI and Win32 Job Object process runner, with private temporary cache, fixed commands, bounded runtime/output and no learner acceptance records. Expose truthful profile in the existing knowledge dialog. Package regression runs all 20 variants and checks classic progress/history byte identity. Source-built native without the package profile, classic knowledge experiments and two editable modern projects remain disabled. Fresh Windows CI required; no modern native PASS claim yet. No course expansion, no Linux fallback or full ISO claim.


### CP7-E2 native editable modern projects (candidate)

E1 at baff74a6 passed 20/20 modern variants, all 101 classic tests and unchanged classic evidence in actual portable run 46010049974; inherited D3 notice test later failed. E2 reuses the existing sample-window (7) and frame-header (8) contracts with no test-body or starter changes. Only the fixture compiler-profile gate is extended to the exact native GCC 16.2.0. The existing native engine supervisor executes fixed CMake/Ninja/CTest phases in a temporary cache, checks actual CMake compiler/sysroot/Ninja/GTest paths, rejects wrong profile, records independent source/contract/profile evidence, and guards Windows local project paths against reparse/hardlink entries. Actual extracted-package HTTP acceptance checks starter failure, all 15 reference test passes, revision conflicts, resume/stale and unchanged classic progress. Linux GCC13 behavior remains; native learner code is trusted local code, not a security sandbox. Physical Windows, browser DOM, native classic knowledge experiments and publication approval remain separate. Fresh CI required.


### CP7-F2 on-demand WSL2 handoff (candidate)

Native W1–W4 remain independent of WSL; existing W5–W9 course selection exposes one explicit handoff action. Inspect supported installed WSL2 only on demand; choose supported default, otherwise 24.04 then 22.04. Launch only an already prepared compatible Ubuntu checkout at ~/work/CppAppLab, with no installation, cloning, update, privilege request, source copying or PASS migration. Fixed local port distinct from native port; compare Linux-side service instance and catalog identity with the Windows loopback endpoint before navigating the existing #task deep link. Linux may still require its own prerequisite acceptance. Native workbench remains running; browser back returns. Fixed argument/receipt tests and real packaged rejection routes are candidate gates, not actual WSL2 integration evidence; real target machine required. F3 evidence revalidation remains open.


### CP7-F2 correction and F3a actual Linux prerequisite navigation (candidate)

Initial #101 portable 46012925103 and Ubuntu build jobs failed because the fixture named its subprocess stub `run`, overriding unittest.TestCase.run. Windows browser regression also correctly detected a changed start-button behavior. Rename the stub, preserve all prior fixture assertions, and update the actual browser test to require the enabled WSL handoff while keeping local build/run/check disabled and checking real HTTP rejection of native Linux execution and injected actions. F3a resolves the complete dependency graph using only Linux engine states, selects the first unmet prerequisite including stale evidence, rejects unknown/contradictory state, and navigates to that course with the requested target retained in the initial link. The actual Linux integration exercises initial lock, real W1 acceptance and subsequent stale source; native requests cannot use this plan to unlock a task. No source or PASS transfer. Explicit safe learner-source reuse and actual WSL2 machine acceptance are separate remaining gates. Fresh CI required.


### CP7-F2/F3a verified; F3b explicit source-only reuse (candidate)

#101 merged at 3fef88433eb2a8b4f91f4cda4b212f65fc9249a5 after portable 46016954035, primary 46016954066 (14/14 applicable) and foundation 46016954268 PASS. Real Windows Chromium handoff guards and Linux prerequisite-state integration passed; 101 classic + 20 fixed modern + 15 modern project contracts unchanged. Actual WSL2 startup/forwarding is still a physical-target gate.

F3b exports current PASSED W1–W4 declared learner source bytes with per-file SHA-256 and normalized catalog identity in one explicit JSON download. No Windows PASS, progress, evidence, build data or arbitrary files are included. Linux imports only the selected unlocked AVAILABLE foundation task, validates schema, exact file set, hash and catalog before creation, uses the normal real engine to create a new workspace, refuses resumed/existing or modified work, stages all bytes before replacing fresh starters, never runs code on import, and requires a separate real Linux check. Ordinary local-app concurrency guard serializes foundation web operations; not a same-user adversarial sandbox or multi-file disk transaction. An interrupted import may leave only its newly created workspace incomplete, with no fabricated acceptance. Existing Linux workspaces remain manual-comparison only. Per-course source limit 512 KiB / encoded 768 KiB; bundle 4 MiB. Explicit UI controls are in existing course information; no design overhaul.

Actual native ZIP export and Chromium download checks plus Linux integration with export-shaped fixture bytes and all 101 real checks are mandatory. W5 must unlock only after Linux checks. This is not a claim of actual Windows-to-WSL transport, ordinary-user machine or human learning acceptance. Public license approval remains open. Fresh CI required.
