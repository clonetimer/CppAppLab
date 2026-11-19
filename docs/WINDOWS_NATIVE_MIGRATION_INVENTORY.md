# Windows-native migration inventory

**Audit baseline:** 2026-11-19  
**Target:** CppAppLab source build, learner package, course acceptance and CI run directly on Windows 11. No WSL, Ubuntu, Linux kernel, Bash runtime or Linux package manager may be required.  
**Quality boundary:** retain real GCC, CMake, GoogleTest/CTest, platform-owned formal acceptance, compiler diagnostics and existing behavior/security contracts. A Windows port is not complete if it hides POSIX tests, weakens checks, or substitutes mocks for the actual learner program.

## Audit scope and method

Audited the default-branch tree (458 tracked files), README and architecture decisions, the sole GitHub Actions workflow, root CMake configuration, repository shell/PowerShell/Python entry points, representative process/network/reactor/filesystem implementations, course fixtures and learner-facing setup/package documents.

Inventory counts:
- 30 Bash shell scripts (.sh)
- 3 PowerShell scripts (.ps1) and one Windows .cmd entry point
- 11 Python files and 5 Node .cjs tests; Python/Node are generally cross-platform, except APT-specific utilities and any embedded POSIX paths/subprocess assumptions
- 40 CMakeLists/CMake module files
- 0 tracked Git symlink entries; symlink/reparse-point security is nevertheless tested by four acceptance fixtures

Effort labels: **Small** = isolated script/test/documentation change; **Medium** = several components or bounded Windows API integration; **Large** = process, socket/reactor, filesystem security or broad learner/reference/fixture port with equivalent semantics and regression coverage.

---

## ① Scripts and shebangs

### Findings

There are 30 Bash scripts, all using Bash shebangs or invoked explicitly through Bash. Windows PowerShell and cmd.exe cannot execute them natively. Git Bash/MSYS Bash is not an acceptable final dependency because the target is a pure Windows-native workflow.

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

**Complete 30-file Bash inventory**

- Course fixtures (13): course/fixtures/w1/calculator/tests/divide_zero.sh; course/fixtures/w2/todo/tests/todo_cli_workflow.sh; course/fixtures/w3/task-manager/tests/task_manager_cli_workflow.sh; course/fixtures/w4/quality-gate/tests/task_manager_cli_workflow.sh; course/fixtures/w5/system-lab/tests/system_lab_cli_smoke.sh; course/fixtures/w6/thread-pool/tests/thread_pool_smoke.sh; course/fixtures/w7/tcp-chat/tests/tcp_chat_smoke.sh; course/fixtures/w7/tcp-echo/tests/tcp_echo_smoke.sh; course/fixtures/w8/http-parser/tests/http_parser_smoke.sh; course/fixtures/w8/http-protocol/tests/http_protocol_smoke.sh; course/fixtures/w8/http-server/tests/http_server_smoke.sh; course/fixtures/w8/router/tests/router_smoke.sh; course/fixtures/w8/static-files/tests/static_files_smoke.sh.
- Learner script (1): labs/w5/system-lab/learner/scripts/quality-check.sh.
- Product/setup/package scripts (14): scripts/accept_learner_package_wsl.sh; scripts/check_environment.sh; scripts/ci_prepare_ubuntu_apt.sh; scripts/ci_prepare_ubuntu_snapshot.sh; scripts/cppapplab-wsl-launcher.sh; scripts/install_offline_dependencies.sh; scripts/lib/terminal.sh; scripts/lib/ubuntu-support.sh; scripts/package_offline_dependencies.sh; scripts/package_release.sh; scripts/quickstart.sh; scripts/smoke_learner_package.sh; scripts/start-web-workbench.sh; scripts/verify_offline_dependency_bundle.sh.
- Test helpers (2): tests/test_ubuntu_support.sh; tests/test_windows_browser_open.sh.

Existing native entry points worth retaining/expanding: Start-CppAppLab.cmd, scripts/Start-CppAppLab.ps1, tests/windows-launcher-smoke.ps1, tests/windows-target-machine-acceptance.ps1. The 11 Python and 5 Node scripts are not inherently Linux-only; audit individual subprocess/path assumptions rather than rewriting them wholesale.

## ② Paths and symbolic links

### Findings

- Tracked symlinks: **0**. No repository file itself is a symlink.
- Symlink/reparse-point behavior is nevertheless part of the product security contract. src/workspace.cpp uses std::filesystem::symlink_status to reject symlinked workspaces, source files and learner-template entries. src/progress.cpp rejects symlinked state files/directories and uses O_NOFOLLOW when creating progress files.
- Fixed acceptance explicitly creates symlinks in four fixture files: course/fixtures/w2/todo/tests/todo_acceptance_test.cpp, course/fixtures/w3/task-manager/tests/task_manager_acceptance_test.cpp, course/fixtures/w4/quality-gate/tests/task_manager_acceptance_test.cpp, and course/fixtures/w9/static-root/tests/secure_static_root_acceptance_test.cpp.
- Linux-only path/runtime assumptions found: /proc/<pid>/cmdline in scripts/start-web-workbench.sh; /dev/null in src/process.cpp, process tests and web integration; /dev/tty in scripts/lib/terminal.sh; /bin/echo, /bin/cat, /bin/sh, /usr/bin/printf in process demonstrations/tests; /tmp in tests/CI; HOME, ~/.local/share, XDG runtime/state variables in progress, Todo workspaces and service PID/log handling; Linux executable mode checks (-x), chmod, and install -m in scripts.
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
- Current jobs run on ubuntu-22.04, ubuntu-24.04, and an Ubuntu matrix. Steps use sudo apt-get, Bash, GNU timeout, /tmp, chmod, base64 -w0, grep, bash -n, /dev/null, Linux paths and Ubuntu-specific package verification.
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

- Root README.md: Ubuntu source checkout, Bash workbench/quickstart commands, ./build/cppapplab, $HOME, ~/cppapplab-work and Linux build/launch assumptions.
- docs/QUICKSTART.md: WSL distribution selection, Ubuntu clone path, Bash quickstart/workbench commands, apt approval, shell package commands and WSL-specific troubleshooting.
- docs/ENVIRONMENT_WSL2_UBUNTU.md: WSL status/version commands, Ubuntu apt, uname, g++, gdb, code ., Linux filesystem guidance and Bash environment checks.
- docs/RELEASE_PACKAGING.md, docs/TARGET_ACCEPTANCE.md: Bash packaging/smoke scripts, tar.gz package names, Ubuntu version checks, APT/offline dependency instructions.
- packaging/LEARNER-README.md, packaging/OFFLINE-DEPENDENCIES-README.md: apt installation, Bash launcher/environment check, .deb, tar extraction and Ubuntu-specific package identity.
- docs/ARCHITECTURE.md, docs/DECISIONS.md, docs/STATUS.md, docs/ROADMAP.md: Linux/WSL as supported platform and explicit Linux-only architecture decisions. Preserve historical decisions as superseded records; add a new Windows-native decision rather than rewriting history.
- course/CURRICULUM.md, course/LEARNING_GUIDE_ZH.md, W5/W7/W8/W9 lesson and hint files: CMake/CTest commands are largely portable, but Bash wrappers, /bin/*, POSIX process/socket/epoll APIs, openat/O_NOFOLLOW, Linux filesystem assumptions and descriptions of Linux-only outcomes must be replaced by Windows-native equivalents while retaining the same learning objectives and acceptance depth.

### Windows-native command replacements

| Current command/pattern | Replacement | Effort |
|---|---|---|
| bash scripts/quickstart.sh, bash scripts/start-web-workbench.sh | .\scripts\Start-CppAppLab.ps1 or .\Start-CppAppLab.cmd; PowerShell subcommands for setup/start/status/stop | Small–Medium |
| sudo apt install ... | Run the pinned toolchain bootstrap/installer and verify hashes/versions | Medium |
| cmake -S . -B build, cmake --build build, ctest --test-dir build | Keep these commands; run from PowerShell with native CMake/Ninja and Windows paths | Small |
| ./build/cppapplab, ./cppapplab | .\build\cppapplab.exe, .\cppapplab.exe | Small |
| tar -xzf, sha256sum --check, chmod +x | Expand ZIP, Get-FileHash -Algorithm SHA256, no executable-bit step for .exe/.ps1 | Small |
| $HOME/.local/share, ~/cppapplab-work | %LOCALAPPDATA%\CppAppLab for progress/state and a documented workspace root under the user's profile | Medium |
| /bin/echo, /bin/cat, /bin/sh | C++ test-helper executable or explicit Windows system executable, launched through the native process runner | Medium |

Historical/archive documents can remain as historical records, but must be clearly labelled as superseded and must not appear in the current Windows quick-start route.

---

## Migration order: smallest safe changes first

This is an ordering plan, not a claim that the work is already implemented.

1. **Inventory and decision baseline (Small).** Keep this file as the audit source of truth; add a new ADR stating Windows 11 native-only support supersedes WSL/Ubuntu. Do not change runtime yet.
2. **Portable test/script surface (Small–Medium).** Convert the 13 course shell smoke wrappers and two shell test helpers to CTest direct commands, Python or PowerShell. Keep every assertion and process exit/output contract.
3. **Native user entry and paths (Medium).** Expand the existing PowerShell launcher/quickstart; centralize LocalAppData/workspace/temp paths; replace Linux service PID/log lifecycle and remove WSL probing from the default path.
4. **Pinned Windows GCC toolchain and CI (Medium–Large).** Build a checksummed native GCC/GDB/CMake/Ninja/GoogleTest toolchain bundle. Add Windows runner validation and retain real GCC + GoogleTest/CTest. Do not mark Windows support complete while the root CMake guard or POSIX sources still fail.
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
- This runner uses native CMake process execution and does not require Bash. It is suitable for both Linux and Windows CMake generators.

This is deliberately only a test-harness migration. The repository root still rejects Windows, and the runtime still contains POSIX APIs; no Windows-native product support is claimed. Next small candidates are the remaining simple CLI smoke wrappers that can be expressed as direct CTest commands or a portable test helper without weakening multi-process persistence assertions.

- Added a dedicated CI step that configures/builds the W1 Calculator platform-owned fixture against the reference implementation and runs its CTest suite. This ensures the new CMake runner is actually executed by CI, not merely parsed.
