# Windows-native migration inventory

**Audit baseline:** 2026-11-19  
**Target:** CppAppLab source build, learner package, course acceptance and CI run directly on Windows 11. No WSL, Ubuntu, Linux kernel, Bash runtime or Linux package manager may be required.  
**Quality boundary:** retain real GCC, CMake, GoogleTest/CTest, platform-owned formal acceptance, compiler diagnostics and existing behavior/security contracts. A Windows port is not complete if it hides POSIX tests, weakens checks, or substitutes mocks for the actual learner program.

## Audit scope and method

Audited the default-branch tree (454 tracked files), README and architecture decisions, the sole GitHub Actions workflow, root CMake configuration, repository shell/PowerShell/Python entry points, representative process/network/reactor/filesystem implementations, course fixtures and learner-facing setup/package documents.

Inventory counts (default-branch baseline before the first migration in this PR):
- 30 Bash shell scripts (.sh) on the original `main` baseline; PR #59 removed the W1 Calculator wrapper, PR #60 removed six W6–W8 wrappers, PR #61 removed three W2–W4 persistence wrappers, and this follow-up removes two W8 integration wrappers, leaving 18 `.sh` files on the current migration branch
- 3 PowerShell scripts (.ps1) and one Windows .cmd entry point
- 13 Python files and 5 Node .cjs tests; Python/Node are generally cross-platform, except APT-specific utilities and embedded POSIX path/subprocess assumptions
- 40 `CMakeLists.txt` files and 4 `.cmake` helper modules (44 CMake files total)
- 0 tracked Git symlink entries; symlink/reparse-point security is nevertheless tested by four acceptance fixtures

Effort labels: **Small** = isolated script/test/documentation change; **Medium** = several components or bounded Windows API integration; **Large** = process, socket/reactor, filesystem security or broad learner/reference/fixture port with equivalent semantics and regression coverage.

---

## ① Scripts and shebangs

### Findings

The original baseline had 30 Bash scripts; 18 remain on current `main`. All remaining `.sh` files use Bash shebangs or are invoked explicitly through Bash. Windows PowerShell and cmd.exe cannot execute them natively. Git Bash/MSYS Bash is not an acceptable final dependency because the target is a pure Windows-native workflow.

| Inventory | Current dependency | Windows-native replacement | Effort |
|---|---|---|---|
| scripts/quickstart.sh, scripts/check_environment.sh, scripts/lib/ubuntu-support.sh, scripts/lib/terminal.sh | Bash arrays, sourcing, terminal prompts, dpkg/Ubuntu checks, uname | PowerShell 7 setup/diagnostics and native console prompts; expand the existing Start-CppAppLab.ps1 entry point | Medium |
| scripts/start-web-workbench.sh | Bash backgrounding/nohup, kill, /proc/<pid>/cmdline, XDG PID/log paths, tail | PowerShell process lifecycle using a process handle, PID plus executable identity, Windows per-user LocalAppData/Temp paths; retain loopback health contract | Medium |
| scripts/cppapplab-wsl-launcher.sh, scripts/accept_learner_package_wsl.sh | WSL bootstrap, Bash path expansion, apt bootstrap, tar extraction, Ubuntu checks | Retire from the default path; replace with native PowerShell clone/setup/package acceptance. Keep only as explicitly deprecated legacy scripts during transition, then remove | Medium |
| scripts/package_release.sh, scripts/smoke_learner_package.sh | Bash packaging, install, chmod, GNU tar/gzip/find/touch/sha256sum | PowerShell/Python package builder, ZIP archive, SHA-256 via .NET; deterministic timestamps/metadata must remain tested | Medium |
| scripts/package_offline_dependencies.sh, scripts/install_offline_dependencies.sh, scripts/verify_offline_dependency_bundle.sh, scripts/build_local_apt_index.py, packaging/download_apt_uris.py, packaging/resolve_apt_uris.py | Bash plus APT/.deb tooling; Python utilities parse/download Debian metadata | Replace Debian bundle with a pinned Windows-native GCC/CMake/GDB/GoogleTest toolchain ZIP and signed/checksummed manifest. Rewrite installer/verifier in PowerShell/Python; do not translate APT into another hidden Linux package manager | Large |
| scripts/ci_prepare_ubuntu_apt.sh, scripts/ci_prepare_ubuntu_snapshot.sh | Ubuntu archive, apt sources, apt keyring and snapshot | Remove after Windows CI uses pinned Windows toolchain artifacts; CI bootstrap verifies archive checksums and compiler versions | Small once native toolchain exists |
| scripts/lib/terminal.sh | /dev/tty, Bash read -p, shell stdin/TTY distinctions | PowerShell Read-Host/Console APIs, with explicit non-interactive behavior | Small |
| tests/test_ubuntu_support.sh, tests/test_windows_browser_open.sh | Bash test harnesses, chmod, mktemp, kill/wait, grep | PowerShell Pester or Python unittest harnesses using native processes and temporary directories | Small |
| Course smoke scripts | Bash argument checking, pipelines, grep and shell process execution | Prefer CTest add_test(COMMAND $<TARGET_FILE:...>) and platform-owned C++/Python subprocess assertions. No shell wrapper is needed for simple executable smoke tests | Small–Medium |
| labs/w5/system-lab/learner/scripts/quality-check.sh | Bash CMake/build/CTest wrapper | CMake preset plus PowerShell helper or direct documented CMake/CTest commands | Small |

**Complete 30-file Bash inventory (baseline `main`, before this PR's deletion)**

- Course fixtures (13): course/fixtures/w1/calculator/tests/divide_zero.sh (removed by PR #59); course/fixtures/w2/todo/tests/todo_cli_workflow.sh (removed by this follow-up); course/fixtures/w3/task-manager/tests/task_manager_cli_workflow.sh (removed by this follow-up); course/fixtures/w4/quality-gate/tests/task_manager_cli_workflow.sh (removed by this follow-up); course/fixtures/w5/system-lab/tests/system_lab_cli_smoke.sh; course/fixtures/w6/thread-pool/tests/thread_pool_smoke.sh (removed by this follow-up); course/fixtures/w7/tcp-chat/tests/tcp_chat_smoke.sh (removed by this follow-up); course/fixtures/w7/tcp-echo/tests/tcp_echo_smoke.sh (removed by this follow-up); course/fixtures/w8/http-parser/tests/http_parser_smoke.sh (removed by this follow-up); course/fixtures/w8/http-protocol/tests/http_protocol_smoke.sh (removed by this follow-up); course/fixtures/w8/http-server/tests/http_server_smoke.sh (removed by this follow-up); course/fixtures/w8/router/tests/router_smoke.sh (removed by this follow-up); course/fixtures/w8/static-files/tests/static_files_smoke.sh (removed by this follow-up).
- Learner script (1): labs/w5/system-lab/learner/scripts/quality-check.sh.
- Product/setup/package scripts (14): scripts/accept_learner_package_wsl.sh; scripts/check_environment.sh; scripts/ci_prepare_ubuntu_apt.sh; scripts/ci_prepare_ubuntu_snapshot.sh; scripts/cppapplab-wsl-launcher.sh; scripts/install_offline_dependencies.sh; scripts/lib/terminal.sh; scripts/lib/ubuntu-support.sh; scripts/package_offline_dependencies.sh; scripts/package_release.sh; scripts/quickstart.sh; scripts/smoke_learner_package.sh; scripts/start-web-workbench.sh; scripts/verify_offline_dependency_bundle.sh.
- Test helpers (2): tests/test_ubuntu_support.sh; tests/test_windows_browser_open.sh.

Existing Windows-side entry points and harnesses are **not native runtime entry points**: `Start-CppAppLab.cmd` and `scripts/Start-CppAppLab.ps1` run on Windows but currently invoke `wsl.exe`, Bash and Ubuntu; they are legacy WSL bridges that must be replaced/re-scoped for native setup and launch. `tests/windows-launcher-smoke.ps1` tests that WSL handoff, while `tests/windows-target-machine-acceptance.ps1` validates Windows + WSL2; neither is evidence of native Windows runtime support. Retain PowerShell as the Windows automation surface, but rewrite the launcher and acceptance harness for a native toolchain. The 13 Python and 5 Node scripts are not inherently Linux-only; audit individual subprocess/path assumptions rather than rewriting them wholesale. Two concrete Python exceptions are `tests/test_interactive_prompt.py` (executes `/bin/bash`) and `tests/test_web_workbench_integration.py` (invokes `bash scripts/start-web-workbench.sh`).

## ② Paths and symbolic links

### Findings

- Tracked symlinks: **0**. No repository file itself is a symlink.
- Symlink/reparse-point behavior is nevertheless part of the product security contract. src/workspace.cpp uses std::filesystem::symlink_status to reject symlinked workspaces, source files and learner-template entries. src/progress.cpp rejects symlinked state files/directories and uses O_NOFOLLOW when creating progress files.
- Fixed acceptance explicitly creates symlinks in four fixture files: course/fixtures/w2/todo/tests/todo_acceptance_test.cpp, course/fixtures/w3/task-manager/tests/task_manager_acceptance_test.cpp, course/fixtures/w4/quality-gate/tests/task_manager_acceptance_test.cpp, and course/fixtures/w9/static-root/tests/secure_static_root_acceptance_test.cpp.
- Linux-only path/runtime assumptions found: `/proc/<pid>/cmdline` in `scripts/start-web-workbench.sh`; `/dev/null` in `src/process.cpp`, `tests/test_interactive_prompt.py` and the W8 HTTP acceptance fixture; the core `tests/process_test.cpp` no longer uses a Unix null device after PR #65; `/dev/tty` in `scripts/lib/terminal.sh`; `/bin/echo`, `/bin/sh` and `/usr/bin/printf` in W5 process demonstrations/tests; `/bin/cat` and the other hard-coded `/bin/*` commands were removed from core `ProcessRunnerTest` by PR #65; `/bin/bash` in `tests/test_interactive_prompt.py` and `bash` subprocess invocation in `tests/test_web_workbench_integration.py`; `/tmp` in tests/CI; `HOME`, `~/.local/share`, XDG runtime/state variables in progress, Todo workspaces and service PID/log handling; Linux executable-mode checks (`-x`), `chmod`, and `install -m` in scripts.
- src/progress.cpp also uses POSIX flock, open, write, fsync, getpid, O_CLOEXEC, O_NOFOLLOW, and file-descriptor semantics. src/workspace.cpp uses portable filesystem APIs, but its security checks need Windows reparse-point/race review rather than assuming symlink equivalence.

| Area | Windows-native replacement | Effort |
|---|---|---|
| Per-user state/workspace roots | SHGetKnownFolderPath(FOLDERID_LocalAppData) or %LOCALAPPDATA%; use std::filesystem::temp_directory_path() for temporary files; centralize path policy instead of duplicating HOME/.local logic in W2–W4 learner/reference CLIs | Medium |
| PID/process identity and logs | Native process handle and executable path verification; LocalAppData/Temp; PowerShell service lifecycle. Do not parse /proc | Small–Medium |
| Process I/O and null device | Win32 anonymous pipes, NUL, process handles and Job Objects; remove hard-coded /bin/* test executables | Large (process runner); Small (test fixtures after runner exists) |
| Symlink checks for ordinary paths | GetFileAttributesW(FILE_ATTRIBUTE_REPARSE_POINT) and handle-based inspection using CreateFileW(FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS) / GetFileInformationByHandleEx(FileAttributeTagInfo) | Medium |
| Secure W9 document-root traversal | Keep the opened root handle; traverse/reopen components without following reparse points and verify the opened handle is a regular file. A lexical prefix check or simple std::filesystem check is not equivalent to openat/O_NOFOLLOW; investigate handle-relative NtCreateFile or another race-safe Win32 design | Large |
| Atomic progress writes and cross-process locking | CreateFileW(CREATE_NEW), FlushFileBuffers, ReplaceFileW/MoveFileExW, LockFileEx, and explicit reparse-point checks | Medium |
| POSIX fd/errno/permissions | RAII wrappers around HANDLE / SOCKET, GetLastError / WSAGetLastError, Windows ACLs and explicit sharing modes | Large across W5–W9 |

Windows symlink creation may require Developer Mode or privilege depending on the target. Tests must use unprivileged-capable symlink creation where supported, and must separately test junction/reparse-point attacks. They must not silently skip the security contract when symlink creation is unavailable.

## ③ Package managers and system tools

### Findings

- Source setup requires Ubuntu package names: build-essential, cmake, git, gdb, libgtest-dev, python3; optional clang-format and clang-tidy.
- CI calls apt-get update/install, apt-cache policy, dpkg-query, dpkg --compare-versions, dpkg-deb, Ubuntu keyrings and signed APT snapshots.
- Offline dependency packaging resolves .deb dependency closures through apt-get --print-uris, downloads and hashes packages, builds a local APT index, and installs with apt-get --no-download. Related files include scripts/ci_prepare_ubuntu_apt.sh, scripts/ci_prepare_ubuntu_snapshot.sh, scripts/package_offline_dependencies.sh, scripts/install_offline_dependencies.sh, scripts/build_local_apt_index.py, packaging/download_apt_uris.py, packaging/resolve_apt_uris.py, packaging/offline-dependencies.txt, packaging/offline-quality-tools.txt, and the two packaging READMEs.

| Current dependency | Windows-native replacement | Effort |
|---|---|---|
| apt / dpkg / .deb / Ubuntu archive snapshots | A pinned Windows x64 toolchain ZIP with a manifest containing compiler/tool versions, source URLs and SHA-256 hashes. Verify/extract with PowerShell/.NET; no package-manager side effects | Large |
| build-essential / GCC | Real native Windows GCC (MinGW-w64/UCRT64 target); ship or provision compiler/runtime DLLs and GDB. Run g++ directly from PowerShell, not from WSL | Medium–Large |
| CMake / Ninja or Make | Official native Windows CMake + Ninja binaries, pinned and checksum-verified | Small |
| GoogleTest system package | Keep the existing pinned GoogleTest CMake FetchContent version (currently v1.18.0), or package the exact source/build artifact. Do not replace it with handwritten assertions or mocks | Small |
| GDB, clang-format, clang-tidy | Include pinned native Windows executables in the toolchain bundle; verify versions and hashes | Medium |
| tar.gz Linux learner/offline bundles | Windows learner ZIP containing the native .exe, course/fixtures/reference data and metadata; if a compiler is needed for formal acceptance, provide the pinned GCC/CMake/GDB/GTest toolchain separately or in an optional toolchain ZIP | Medium |

winget may be offered as a convenience for a developer workstation, but it should not be the CI/release source of truth because mutable package versions and machine state weaken reproducibility. Do not substitute MSYS Bash for the product runtime. The native GCC distribution itself must produce Windows PE executables and run without WSL.

## ④ CI runners and build environment

### Findings

- Only workflow: .github/workflows/ci.yml.
- Current runners: `ubuntu-22.04` push-only source compatibility; `ubuntu-22.04`/`ubuntu-24.04` PR matrices for GCC build/GoogleTest, ASan/UBSan, clang-tidy, package/offline checks and Chromium; plus `windows-latest` (`windows-launcher-syntax`).
- The Windows job only parses the existing PowerShell launcher, exercises Windows-to-WSL handoff through a stub, and syntax-checks legacy Bash scripts with Git Bash. It does not configure root CMake, compile learner C++ with Windows GCC, or run Windows CTest; there is currently **no native Windows C++ build/acceptance job**. Ubuntu steps use sudo apt-get, Bash, GNU timeout, /tmp, chmod, base64 -w0, grep, bash -n, /dev/null, Linux paths and Ubuntu-specific package verification.
- Root CMakeLists.txt explicitly terminates configuration unless CMAKE_SYSTEM_NAME is Linux. The repository contains 40 CMakeLists/CMake module files; CMake itself is portable, but task sources and test fixtures are not yet Windows-buildable.
- Root CMake already has MSVC warning flags, but that does not establish Windows support: core src/process.cpp and W5–W9 labs include POSIX headers and APIs.
- CI currently validates real GCC, GoogleTest/CTest, ASan/UBSan, clang-format/clang-tidy, browser acceptance and task-specific fixtures on Ubuntu. These gates must be re-established on Windows rather than simply deleted.

| Current dependency | Windows-native replacement | Effort |
|---|---|---|
| runs-on: ubuntu-22.04/24.04 | windows-2025 or another explicitly pinned Windows runner; record OS/image and toolchain versions | Small for runner declaration; large for passing all tests |
| Ubuntu apt installation | Download/cache a pinned native GCC toolchain archive; verify SHA-256 before use | Medium |
| Bash workflow steps | PowerShell 7 steps, CMake presets and Python test helpers; use native Windows paths and quoting | Medium |
| Root CMake Linux-only fatal guard | Introduce a real Windows platform target after native platform layers exist; keep compiler ID checks and fail-fast unsupported targets | Small code diff, large integration |
| POSIX-only source/fixture targets | Build and run the same platform-owned test fixtures against native Windows implementations; keep real GCC + CMake + GoogleTest/CTest | Large |
| Ubuntu sanitizer/quality matrix | Validate actual Windows GCC ASan/UBSan availability; if unsupported, do not report a skipped check as passed. Preserve equivalent GCC, GoogleTest, clang-format/clang-tidy gates and report any platform-specific sanitizer limitation explicitly | Medium–Large |

Recommended CI baseline: Windows x64 runner + pinned native MinGW-w64 GCC + native CMake/Ninja + pinned GoogleTest source + GDB. The compiler must be the actual GCC executable, not MSVC or a fake compiler wrapper. Verify generated binaries are native Windows executables and execute them on the Windows runner.

## ⑤ Commands in documentation

### Primary user-facing documents requiring conversion

- Root README.md: Ubuntu source checkout, Bash workbench/quickstart commands, `./build/cppapplab`, `$HOME`, `~/cppapplab-work` and Linux build/launch assumptions.
- docs/DEVELOPMENT.md: says to build from Ubuntu and uses a Linux `find`/`xargs` pipeline for clang-format; convert the environment wording and file enumeration while keeping the CMake/CTest commands.
- docs/LEARNING_ENGINE_DESIGN.md: records the W5 POSIX process runner and Linux-only task acceptance boundary; update the implementation/platform descriptions without changing the task contract.
- docs/QUICKSTART.md: WSL distribution selection, Ubuntu clone path, Bash quickstart/workbench commands, apt approval, shell package commands and WSL-specific troubleshooting.
- docs/ENVIRONMENT_WSL2_UBUNTU.md: WSL status/version commands, Ubuntu apt, uname, g++, gdb, code ., Linux filesystem guidance and Bash environment checks.
- docs/RELEASE_PACKAGING.md, docs/TARGET_ACCEPTANCE.md: Bash packaging/smoke scripts, tar.gz package names, Ubuntu version checks, APT/offline dependency instructions.
- packaging/LEARNER-README.md, packaging/OFFLINE-DEPENDENCIES-README.md: apt installation, Bash launcher/environment check, .deb, tar extraction and Ubuntu-specific package identity.
- docs/ARCHITECTURE.md, docs/DECISIONS.md, docs/STATUS.md, docs/ROADMAP.md: Linux/WSL as supported platform and explicit Linux-only architecture decisions. Preserve historical decisions as superseded records; add a new Windows-native decision rather than rewriting history.
- course/CURRICULUM.md and course/LEARNING_GUIDE_ZH.md: remove WSL/Ubuntu as the future supported setup path once native setup is ready; retain course coverage and acceptance boundaries.
- course/lessons/w2/todo.md and course/lessons/w3/task-manager.md: replace shell-specific `mkdir -p`, `export`, `$PWD` and `./build/...` examples with PowerShell directory/environment-variable/executable commands.
- course/lessons/w4/quality-gate.md and `labs/w4/quality-gate/learner/README.md`: replace the Linux `find`/`xargs` clang-format pipeline with a native PowerShell or Python file-enumeration command that propagates formatter failures.
- W5/W7/W8/W9 lesson and hint files: CMake/CTest commands are largely portable, but Bash wrappers, `/bin/*`, POSIX process/socket/epoll APIs, `openat`/`O_NOFOLLOW`, Linux filesystem assumptions and descriptions of Linux-only outcomes must be replaced by Windows-native equivalents while retaining the same learning objectives and acceptance depth.

### Windows-native command replacements

| Current command/pattern | Replacement | Effort |
|---|---|---|
| bash scripts/quickstart.sh, bash scripts/start-web-workbench.sh | .\scripts\Start-CppAppLab.ps1 or .\Start-CppAppLab.cmd; PowerShell subcommands for setup/start/status/stop | Small–Medium |
| sudo apt install ... | Run the pinned toolchain bootstrap/installer and verify hashes/versions | Medium |
| cmake -S . -B build, cmake --build build, ctest --test-dir build | Keep these commands; run from PowerShell with native CMake/Ninja and Windows paths | Small |
| ./build/cppapplab, ./cppapplab | .\build\cppapplab.exe, .\cppapplab.exe | Small |
| tar -xzf, sha256sum --check, chmod +x | Expand ZIP, Get-FileHash -Algorithm SHA256, no executable-bit step for .exe/.ps1 | Small |
| find/xargs-based clang-format enumeration | PowerShell `Get-ChildItem -Recurse -File` or a Python helper that passes file paths as an argument array and propagates nonzero formatter exits | Small |
| `mkdir -p`, `export VAR=...`, `./build/program` in W2/W3 lessons | `New-Item -ItemType Directory -Force`, `$env:VAR = ...`, `.\build\program.exe` | Small |
| $HOME/.local/share, ~/cppapplab-work | %LOCALAPPDATA%\CppAppLab for progress/state and a documented workspace root under the user's profile | Medium |
| /bin/echo, /bin/cat, /bin/sh | C++ test-helper executable or explicit Windows system executable, launched through the native process runner | Medium |

Historical/archive documents can remain as historical records, but must be clearly labelled as superseded and must not appear in the current Windows quick-start route.

---

## Migration order: smallest safe changes first

This is an ordering plan, not a claim that the work is already implemented.

1. **Inventory and decision baseline (Small).** Keep this file as the audit source of truth; add a new ADR stating Windows 11 native-only support supersedes WSL/Ubuntu. Do not change runtime yet.
2. **Portable test/script surface (Small–Medium).** Convert the one remaining course shell wrapper (W5 process execution) and two shell test helpers to CTest, Python or PowerShell. PR #59 removed W1; PR #60 removed W6 ThreadPool, W7 TCP Chat/Echo and W8 HTTP Parser/Protocol/Static Files; PR #61 removed the W2–W4 multi-process persistence wrappers; this follow-up removes the W8 HTTP server and Router wrappers. Keep every assertion and process exit/output contract.
3. **Native user entry and paths (Medium).** Expand the existing PowerShell launcher/quickstart; centralize LocalAppData/workspace/temp paths; replace Linux service PID/log lifecycle and remove WSL probing from the default path.
4. **Pinned Windows GCC toolchain and CI (Medium–Large).** Build a checksummed native GCC/GDB/CMake/Ninja/GoogleTest toolchain bundle. Replace the current `windows-launcher-syntax` WSL-bridge check with native Windows configure/build/CTest acceptance on a pinned Windows runner; keep legacy WSL checks only as explicitly temporary during migration. Retain real GCC + GoogleTest/CTest. Do not mark Windows support complete while the root CMake guard or POSIX sources still fail.
5. **Core process/progress/workspace layer (Large).** Port process execution to CreateProcessW + pipes + Job Objects; locking/atomic state writes to LockFileEx and Win32 file APIs; preserve timeout, output bounds, no-shell execution, no-follow/reparse-point security and persistence tests.
6. **W7 Winsock (Large).** Port TCP echo/chat to Winsock while retaining real loopback sockets, partial I/O, backpressure, disconnect and shutdown tests.
7. **W8 IOCP reactor and timers (Large).** Port epoll/eventfd/timerfd semantics to IOCP, completion/wakeup handles and waitable timers. Retain concurrency, deadline, quota, graceful-shutdown, observability and load/soak contracts.
8. **W9 secure static root (Large).** Implement handle-based, reparse-point-safe traversal and regular-file verification; retain symlink/junction escape and race-oriented security tests.
9. **Package/docs/release closeout (Medium).** Produce native Windows ZIP/package and rewrite current setup, troubleshooting, course and release commands. Publish only after the same formal acceptance gates pass on Windows.

## Non-negotiable acceptance gates

- Native Windows 11 execution with no WSL, Ubuntu, Linux kernel, Bash or apt/dpkg installed.
- Real native Windows GCC/G++ compiles learner source; CMake configures/builds; GoogleTest/CTest runs platform-owned fixtures.
- All 23 task contracts remain present; no task is hidden, downgraded, mocked or marked passed because a platform gate was skipped.
- Preserve process timeouts, output caps, no-shell argument passing, progress locking/atomicity, symlink/reparse-point rejection, real TCP loopback behavior, HTTP deadlines/Keep-Alive/shutdown/observability and W9 path-confinement guarantees.
- Evidence records compiler identity/version, CMake version, GoogleTest version, Windows build, exact commit and per-fixture results.
- WSL scripts and Ubuntu artifacts may be removed only after the native path is independently validated; they must not remain hidden runtime prerequisites.


## First small migration step (in progress)

The first isolated Bash dependency has now been removed without changing the acceptance contract:

- Removed course/fixtures/w1/calculator/tests/divide_zero.sh.
- Added course/fixtures/w1/calculator/tests/assert_divide_zero.cmake.
- Changed the W1 Calculator fixture to invoke the learner executable through CMake execute_process and CTest, checking the exact exit code (3), empty stdout, exact stderr diagnostic and a bounded timeout.
- This runner uses CMake `execute_process` and does not require Bash. Its contract is designed to be platform-neutral; this PR's CI currently executes it on Ubuntu only. Actual Windows execution remains unverified until the native GCC/GoogleTest toolchain and Windows CI job are implemented.

This is deliberately only a test-harness migration. The repository root still rejects Windows, and the runtime still contains POSIX APIs; no Windows-native product support is claimed. PR #59 removed W1; PR #60 removed six W6–W8 CLI smoke wrappers; PR #61 removed the W2–W4 multi-process persistence wrappers; this follow-up replaces the W8 HTTP server and Router shell wrappers with Python 3 tests. The only remaining course shell wrapper is W5 process execution, which must wait for the native Windows process backend so its real child-process semantics are preserved.

- Added a dedicated CI step that configures/builds the W1 Calculator platform-owned fixture against the reference implementation and runs its CTest suite. This ensures the Bash-free CMake runner is actually executed in the current Ubuntu PR matrix, not merely parsed; it is not a Windows-native CI result.

## Status update — 2026-11-19

- The repository currently contains 18 tracked `.sh` files. The W1, W2–W4 and W6–W8 CLI smoke wrappers already converted to CMake/CTest or Python remain converted; this update does not delete or rename legacy WSL/Ubuntu assets.
- PR #65 adds a native C++ child-process fixture and removes `/bin/*` assumptions from the core process-runner tests. Its Ubuntu 22.04 and 24.04 GCC/GoogleTest runs each pass 100/100 root CTest cases.
- The only remaining course shell smoke wrapper is still `course/fixtures/w5/system-lab/tests/system_lab_cli_smoke.sh`. It deliberately remains unchanged because it tests real child-process behavior through `/bin/echo` and `/bin/sh`; convert it only after the Windows process backend exists and the same exit/output/timeout contract can be tested with native Windows child processes.
- This is not a native Windows acceptance result. Root CMake still rejects Windows; `src/process.cpp`, `src/progress.cpp`, `src/learning.cpp`, W7 Winsock/IOCP and W8/W9 POSIX sources still block a Windows build. The next mainline implementation is the Win32 process runner (CreateProcessW, inherited pipes, bounded I/O, timeout and Job Objects), followed by native progress/state handling and a pinned Windows GCC CI path.

## Verified residual dependency audit — 2026-11-19 (post PR #65)

**Audited commit:** `5ebf8ea4a3e62793a47fdb17c13a1d3214d2a921`  
**Counts from the recursive Git tree:** 454 tracked files; 18 Bash `.sh`; 3 PowerShell `.ps1` plus one `.cmd`; 13 Python `.py`; 5 Node `.cjs`; 40 `CMakeLists.txt` plus 4 `.cmake`; zero tracked symlinks. The 40 CMakeLists are distributed across the root, course fixtures, and learner/reference projects.  
**Scope distinction:** the list below separates hard Linux/WSL dependencies from portable code that still needs Windows-specific security or lifecycle behavior. Historical design records are evidence, not current setup instructions; retain them and mark/supersede rather than silently rewriting history.

### A. Exact remaining shell and launcher surface

| Group | Current tracked files | Windows-native disposition | Effort |
|---|---|---|---|
| WSL bootstrap and acceptance | `Start-CppAppLab.cmd`, `scripts/Start-CppAppLab.ps1`, `scripts/cppapplab-wsl-launcher.sh`, `scripts/accept_learner_package_wsl.sh`, `tests/windows-launcher-smoke.ps1`, `tests/windows-target-machine-acceptance.ps1` | Replace the default launcher with native PowerShell setup/start/status/stop; replace WSL target acceptance with clean Windows 11 acceptance. Keep old WSL assets as explicitly legacy until native acceptance passes. | Medium |
| Setup and environment | `scripts/quickstart.sh`, `scripts/check_environment.sh`, `scripts/lib/ubuntu-support.sh`, `scripts/lib/terminal.sh` | Native PowerShell 7 environment detection, console prompts and bootstrap; do not call WSL or Bash. | Medium |
| Web service lifecycle | `scripts/start-web-workbench.sh` | PowerShell process supervision, LocalAppData logs/PID metadata, executable identity and loopback health checks. Remove `/proc`, `nohup`, `tail`, XDG and shell job-control assumptions. | Medium |
| Ubuntu/APT preparation | `scripts/ci_prepare_ubuntu_apt.sh`, `scripts/ci_prepare_ubuntu_snapshot.sh`, `scripts/install_offline_dependencies.sh`, `scripts/package_offline_dependencies.sh`, `scripts/verify_offline_dependency_bundle.sh`, `scripts/build_local_apt_index.py`, `packaging/download_apt_uris.py`, `packaging/resolve_apt_uris.py` | Replace .deb/APT closure and local APT index with a pinned Windows x64 toolchain archive plus a versioned SHA-256 manifest and offline verifier. No Linux package manager in the product path. | Large |
| Release packaging | `scripts/package_release.sh`, `scripts/smoke_learner_package.sh` | PowerShell/.NET ZIP packaging, deterministic metadata, SHA-256 verification, and native executable launch smoke. | Medium |
| Remaining course/test shell | `course/fixtures/w5/system-lab/tests/system_lab_cli_smoke.sh`, `labs/w5/system-lab/learner/scripts/quality-check.sh`, `tests/test_ubuntu_support.sh`, `tests/test_windows_browser_open.sh` | Convert W5 smoke only after the real Windows process runner exists; make the W5 quality check a CMake preset/PowerShell command; replace the two shell test helpers with Python/PowerShell tests or retire obsolete WSL-only coverage alongside the launcher. | Small–Large, depending on W5 process contract |

### B. C++ runtime and course implementation dependencies

| Area and exact code surfaces | Linux/WSL-only behavior | Windows-native implementation required | Effort |
|---|---|---|---|
| Core process runner: `src/process.cpp` | `fork`, `execvp`, `pipe`, `poll`, `waitpid`, process groups/signals and `/dev/null` | `CreateProcessW`, explicit UTF-8/UTF-16 argv quoting, inherited anonymous pipes, bounded concurrent stdin/stdout handling, process handles, timeout, and Job Object tree termination. Preserve no-shell execution and all current limits. | Large |
| W5 process lab: `labs/w5/system-lab/reference/src/process_runner.cpp`; W5 fixture `course/fixtures/w5/system-lab/tests/system_lab_cli_smoke.sh` | Reference runner uses `fork/execvp/waitpid/poll`; fixture invokes `/bin/echo` and `/bin/sh` through Bash. | Teach and accept a real Win32 child-process runner; use the native child fixture and CTest/GoogleTest to assert exit code, stdout/stderr, timeout and cleanup. Do not replace the process exercise with a mock. | Large |
| Core learning runtime: `src/learning.cpp` | `unistd.h`, `isatty`, `gmtime_r`, `setenv/unsetenv`; emits Unix launcher/build commands and shell-oriented log guidance. | `_isatty`/console APIs, `gmtime_s`, `_putenv_s` or explicit child environment blocks, native `.exe` command examples and PowerShell-safe guidance. | Medium |
| Progress persistence: `src/progress.cpp` | POSIX `open/write/fsync/flock/getpid`, `O_CLOEXEC/O_NOFOLLOW`, `gmtime_r`. | `CreateFileW`, exclusive creation, `LockFileEx`, `FlushFileBuffers`, atomic `ReplaceFileW/MoveFileExW`, process-safe unique temporary names and handle-based reparse-point checks. Store default state under `FOLDERID_LocalAppData`. | Large |
| Workspace and evidence safety: `src/workspace.cpp` | Uses portable `std::filesystem` but relies on symlink status checks that do not fully prove Windows reparse-point safety or race resistance. | Inspect opened handles and file attributes/tags; reject symlinks, junctions and other reparse points without a check-then-open race. Preserve no-overwrite and source-fingerprint guarantees. | Medium–Large |
| Core runtime integration tests: `tests/application_runtime_workflow_test.cpp`, `tests/progress_test.cpp`, `tests/*_workflow_test.cpp` | POSIX sockets, `fork/waitpid`, `setenv/unsetenv`, Unix process termination and environment assumptions. | Winsock test sockets; native process fixture/CreateProcessW; scoped environment helpers using Windows APIs; retain actual independent-process and loopback behavior. | Large |
| W7 TCP Echo: `labs/w7/tcp-echo/{learner,reference}/src/{main.cpp,tcp_echo.cpp}`; W7 Chat: `labs/w7/tcp-chat/reference/src/tcp_chat.cpp` | POSIX descriptors, `arpa/inet.h`, `sys/socket.h`, nonblocking `fcntl`, `poll`; Chat additionally uses `epoll` and `eventfd`. | Winsock 2 (`WSAStartup`, `SOCKET`, `closesocket`, `WSAGetLastError`), nonblocking socket operations and a Windows event/completion model. | Large |
| W8 HTTP server variants: `labs/w8/http-server/reference/src/http_server.cpp`, `labs/w8/http-keep-alive/learner/src/http_server.cpp`, `labs/w8/http-connection-guards/learner/src/http_server.cpp`, `labs/w8/http-observability/learner/src/http_server.cpp`, `labs/w8/http-graceful-shutdown/learner/src/http_server.cpp`; load probe: `labs/w8/http-load-probe/reference/src/http_load_probe.cpp` | POSIX sockets plus `epoll`, `eventfd`, `timerfd`, `poll`, descriptor close/error semantics. | Winsock + IOCP completion ports, explicit wakeup/completion signaling and waitable timers. Preserve connection quotas/deadlines, Keep-Alive, graceful shutdown, JSONL observability, metrics and load/soak contracts. | Large |
| W9 secure static root: `labs/w9/static-root/{learner,reference}/src/static_file_handler.cpp` | `openat`, `O_NOFOLLOW`, directory descriptors and POSIX regular-file checks. | Handle-based Windows path traversal with reparse-point-safe opens and final-handle identity/type verification. A string-prefix or `std::filesystem`-only check is insufficient. | Large |
| Browser learner process supervisor: `web/server/cppapplab_web.py` | Python `start_new_session`, `os.killpg`, POSIX process-group signals. | Windows process creation flags plus a Job Object (prefer a small `ctypes` wrapper if no new dependency is justified); terminate the full descendant tree on stop, timeout, task switch and server shutdown. | Medium–Large |

**Already reduced by PR #65:** `tests/process_test.cpp` no longer relies on `/bin/echo`, `/bin/cat`, `/bin/sleep` or `/bin/sh`; it launches the native C++ `tests/process_test_child.cpp`. This is test-harness portability only. `src/process.cpp` and the W5 learner/reference process implementations remain POSIX and are not yet Windows-native.

### C. Python and test harness exceptions

| File | Remaining platform assumption | Replacement |
|---|---|---|
| `tests/test_interactive_prompt.py` | `pty.fork`, `/dev/tty`, `os.waitpid`, Unix session/process semantics | Windows ConPTY/console test harness or a platform-specific native PowerShell test; preserve the redirected-stdin/interactive-confirmation contract. |
| `tests/test_web_workbench_integration.py` | Starts the service with `bash scripts/start-web-workbench.sh`; assumes POSIX process lifecycle. | Invoke the native PowerShell launcher/service entry point and assert the same health/start/stop behavior. |
| `web/server/cppapplab_web.py` | Process groups and Unix signals (see table B). | Job Object-backed process-tree supervision; keep Python standard-library HTTP and socket code where already cross-platform. |
| `course/fixtures/w8/http-server/tests/http_server_smoke.py`, `course/fixtures/w8/router/tests/router_smoke.py` | Python subprocess and loopback sockets, but no inherent Bash dependency. | Retain Python 3 bytes/socket tests; fix only Windows executable suffix, process cleanup or socket behavior where proven necessary. |
| `scripts/build_local_apt_index.py`, `packaging/*.py` APT utilities | Debian package metadata and APT-specific semantics, not Python itself. | Retire from the native product path after a manifest-based Windows toolchain bundle is qualified. |

### D. Build, CI and toolchain

| Surface | Current state | Required Windows-native state |
|---|---|---|
| Root `CMakeLists.txt` | Explicit fatal guard rejects every non-Linux target; root core then compiles POSIX `process.cpp` and `progress.cpp`. | Keep the guard until the core platform backends and all registered tests can configure; then introduce a real `WIN32` target, not a guard deletion that merely exposes compile failures. |
| 40 project `CMakeLists.txt` + 4 CMake helpers | CMake itself is portable; many task fixtures point at POSIX W5–W9 source/test files. | Retain all 23 task targets and fixed acceptance. Port source and fixture dependencies; do not omit targets from Windows CTest to obtain a green build. |
| `.github/workflows/ci.yml` | Ubuntu 22.04/24.04 jobs install via APT and use Bash, GNU `timeout`, `/tmp`, `chmod`, grep and Linux path conventions. Windows job currently parses the PowerShell WSL bridge, simulates WSL argument handoff and uses Git Bash for shell syntax; it does not build the C++ product with Windows GCC. | Add a Windows 11 runner using pinned native MinGW-w64/UCRT GCC, CMake/Ninja and GoogleTest; compile native PE executables and execute the same platform-owned acceptance. PowerShell should orchestrate Windows CI; no Git Bash/WSL runtime in the product test path. |
| Compiler and tools | GCC/GoogleTest/CTest are real on Ubuntu; no pinned Windows compiler bundle or native Windows C++ CI evidence exists. | Native Windows GCC/G++, GDB, CMake/Ninja, GoogleTest and optional clang-format/clang-tidy in a versioned/checksummed archive. Verify compiler identity and PE output. |
| Offline/release package | Ubuntu `.deb` dependency closure and `.tar.gz` learner package; APT snapshots and package signatures. | Windows ZIP with explicit architecture/tool versions, SHA-256 manifest, deterministic packaging and offline verification. |

### E. Documentation and course commands

**Current setup and product route (must be rewritten for Windows after native launcher/toolchain are ready):**
- `README.md`, `docs/QUICKSTART.md`, `docs/ENVIRONMENT_WSL2_UBUNTU.md`: WSL distribution selection, Ubuntu clone paths, Bash quickstart/workbench commands, apt installation and Linux binary invocation.
- `docs/DEVELOPMENT.md`, `docs/RELEASE_PACKAGING.md`, `docs/TARGET_ACCEPTANCE.md`: Ubuntu source build, Bash packaging/smoke commands, tar.gz/.deb and Ubuntu acceptance.
- `packaging/LEARNER-README.md`, `packaging/OFFLINE-DEPENDENCIES-README.md`: apt, .deb, tar extraction and Ubuntu package identity.
- `scripts/Start-CppAppLab.ps1` and `Start-CppAppLab.cmd` usage text: currently documents/launches the WSL handoff, not a native executable.

**Course implementation and copyable command surfaces (retain all learning outcomes, port the APIs and examples):**
- W2/W3: `course/lessons/w2/todo.md`, `course/lessons/w3/task-manager.md`, W2–W4 learner READMEs — shell directory/environment commands, `HOME`/Linux workspace paths and executable invocations.
- W4: `course/lessons/w4/quality-gate.md`, `labs/w4/quality-gate/learner/README.md` — Linux `find/xargs` formatting command.
- W5: `course/lessons/w5/system-lab.md`, hints and W5 learner README — POSIX `fork/exec/waitpid/pipe/poll`, `/bin/*` executables and Bash quality-check commands.
- W7: `course/lessons/w7/tcp-echo.md`, `course/lessons/w7/tcp-chat.md`, hints and W7 READMEs — POSIX socket/epoll terminology and commands; teach Winsock/IOCP equivalents without dropping protocol concepts.
- W8: all `course/lessons/w8/*.md` and W8 learner/reference READMEs — epoll/eventfd/timerfd, descriptor and Linux socket semantics.
- W9: `course/lessons/w9/*.md`, hints and W9 READMEs — openat/O_NOFOLLOW and POSIX path-confinement implementation.
- `course/CURRICULUM.md`, `course/LEARNING_GUIDE_ZH.md`, `course/README.md`, `labs/README.md`: future platform wording and course API map.

**Historical/architecture records (retain and label as historical/superseded, do not delete):**
- `docs/DECISIONS.md` ADR-0001 and ADR-0006 remain historical; ADR-0007 already records Windows 11 native-only as the accepted target.
- `docs/ARCHITECTURE.md`, `docs/STATUS.md`, `docs/ROADMAP.md`, archived status, P8 closeout and design/planning documents should distinguish historical WSL delivery evidence from current native migration status.

### F. Execution order and acceptance boundaries

1. **Inventory baseline — complete:** corrected counts and exact residual files above are the source of truth. PR #65 is merged; the core process test harness is now portable.
2. **Win32 process backend — next:** implement `CreateProcessW`, restricted inherited handles, concurrent bounded pipes, timeout and Job Object descendant cleanup. Run the unchanged `ProcessRunnerTest` contract on Windows with real GCC and the real child executable. Keep root CMake's Linux guard until this isolated Windows process target passes.
3. **Core state and workspace:** port environment/console/time helpers, progress locking/atomic writes and reparse-point-safe workspace operations; run the real persistence/security tests.
4. **W7 networking:** Winsock port and real loopback acceptance.
5. **W8 reactor:** IOCP/wakeup/timer implementation and complete lifecycle/load regression.
6. **W9 static root:** handle-based reparse-point-safe traversal and junction/symlink security tests.
7. **Native launcher, toolchain bundle, CI and release:** replace WSL/APT path with PowerShell + pinned Windows GCC/CMake/GDB/GoogleTest ZIP; then remove/retire legacy WSL assets only after a clean Windows 11 machine passes all 23 task contracts.
8. **Documentation closeout:** update current entry paths and course commands; preserve historical decisions and migration evidence.

**Do not call the project Windows-native until all 23 task contracts, real GCC/CMake/GoogleTest/CTest, process/network/filesystem security, packaging and first-use launch pass on a clean Windows 11 machine and in Windows CI. A skipped target is not a pass.**
