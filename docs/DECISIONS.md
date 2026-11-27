# Architecture Decisions

## ADR-0001 — WSL2 Ubuntu is the first-version acceptance environment

**Decision:** use Windows 11 + WSL2 + Ubuntu 24.04 LTS as the complete v1 acceptance environment.

**Reason:** W5–W8 intentionally teach Linux/POSIX interfaces, sockets, nonblocking I/O, and epoll. A Windows-native-only environment would require either a second implementation or a reduced curriculum.

**Consequence:** MSVC parity is not a v1 acceptance condition. Portable modules may gain native Windows builds later; Linux-specific labs remain Linux-specific.

## ADR-0002 — C++17 is the language baseline

**Decision:** use C++17 for all v1 course projects unless a task explicitly compares a later standard.

**Reason:** this supports the planned STL, RAII, smart pointer, move semantics, concurrency, and networking topics while keeping the baseline consistent with cppStudy.

## ADR-0003 — Reference and learner implementations are separate

**Decision:** every cumulative exercise keeps complete reference source separate from learner-editable source.

**Reason:** learners need real implementation work, while acceptance must remain reproducible and independent of learner modifications.

## ADR-0004 — Platform-owned tests are the acceptance authority

**Decision:** learner tests can supplement but cannot replace platform-owned acceptance fixtures.

**Reason:** the platform must detect behavior, boundary, and error-path regressions even if the learner edits local tests or build files.

## ADR-0005 — A pass is tied to a source fingerprint

**Decision:** evidence records identify the exact source state and build configuration checked.

**Reason:** old successful output must not be presented as a pass after the learner changes the implementation.

## ADR-0006 — Support Ubuntu 22.04 and 24.04 for source builds

**Decision:** CppAppLab source builds and course acceptance support Windows 11 + WSL2 with Ubuntu 22.04 LTS or Ubuntu 24.04 LTS.

**Reason:** the course baseline is C++17, the source requires CMake 3.18 or newer, and both supported LTS releases provide a compatible GCC/CMake toolchain and the Linux/POSIX interfaces used by W5–W9. CI verifies the Ubuntu 22.04 source-build path and retains the complete opt-in validation matrix for both releases.

**Consequence:** the Windows launcher, quick-start script, environment checker, and source-build documentation accept both releases. The prebuilt learner package and offline dependency bundle remain separately versioned Ubuntu 24.04 artifacts until a matching Ubuntu 22.04 package is built and qualified.


## ADR-0007 — Windows 11 native is the sole supported runtime target

**Status:** Historical accepted direction; implementation paused by the user. Superseded by ADR-0008 for CP0–CP6 and ADR-0009 for CP7.

**Decision:** The product target is Windows 11 running CppAppLab directly as a native Windows application. WSL, Ubuntu, a Linux kernel, Bash, apt/dpkg and a POSIX compatibility runtime are not product prerequisites. This decision supersedes the runtime-environment choices in ADR-0001 and ADR-0006; those records remain unchanged as historical decisions.

**Preserved engineering requirements:**
- Native Windows GCC/G++ is the compiler of record; C++17 remains the language baseline.
- CMake remains the build system.
- GoogleTest and CTest remain the formal acceptance framework.
- Platform-owned acceptance contracts, source fingerprints, process timeouts/output bounds, filesystem safety, real loopback TCP behavior and HTTP lifecycle/security checks must remain at least as strict.
- W5–W9 retain their learning outcomes. Their Linux/POSIX-specific implementation exercises are ported to equivalent Windows process, Winsock, IOCP/timer and reparse-point-safe filesystem APIs; they are not removed or replaced by mocks.

**Migration policy:** Keep WSL/Ubuntu scripts and packages only as temporary legacy assets while the native path is being implemented. They must not be required by the Windows path. Remove or archive them only after the native Windows build, toolchain bootstrap, learner package and all 23 task acceptance contracts pass on a clean Windows 11 machine and in Windows CI.

**Reason:** The product should install, build, run, teach and formally accept learner code on Windows without a hidden Linux environment. Portability must be demonstrated by native executables and platform-owned tests, not by a shell compatibility layer.

## ADR-0008 — Freeze the current learning baseline; prioritize teaching usability

**Status:** Historical CP0–CP6 decision (2026-11-26); CP7 direction is ADR-0009.

**Decision:** Preserve the 23 C++17 W1–W9 tasks, prerequisite graph, declared editable files and fixed platform-owned acceptance contracts. The active implementation is Ubuntu 22.04/24.04, including WSL2 startup on Windows. Suspend ADR-0007 implementation; keep the inventory, branch and Draft PR #67 intact. Do not claim a native Windows product or target-machine acceptance.

**Reason:** The user's active priority is understandable browser teaching, using the lessons learned from RustAppLab, not an operating-system migration. UI changes may improve presentation but cannot change compiler, source fingerprint, workspace isolation or tests. Modern C++20/23 work, when started, must use separately verified compiler feature/toolchain profiles and not raise the C++17 baseline.

**Execution:** See MODERN_CPP_LEARNING_PLAN.md. Freeze scope by phase, use actual browser/engine results and the same-state before/after captures, merge only after applicable CI. A passing program is not evidence of independent learner comprehension.


## ADR-0009 — Native Windows foundations plus preserved Linux specialty route

**Status:** Accepted development direction (2026-11-27); implementation candidate, not a supported native product yet.

**Decision:** Windows 11 x64 W1–W4 and existing representative modern C++ content are the first native targets. W5–W9 stay on Ubuntu 22.04/24.04 (including WSL2). Preserve 25 registered C++17 tasks and all fixed contracts. Use native MinGW-w64 UCRT GCC, CMake and GoogleTest. No Bash, WSL or Linux runtime for native execution. Environment availability is separate from prerequisite state and formal PASS. Do not automatically import cross-environment evidence without verified equivalence. CP7 does not require Winsock/IOCP or new courses. Reuse the reviewed isolated Win32 process backend from #67 selectively, not its old branch wholesale. Full root engine, persistence, path safety, actual learner workflow, tooling, browser, offline package and original Linux regression are distinct gates. Real target machine/human results remain post-delivery feedback and must not be represented as passed.

See [CP7 plan](CP7_WINDOWS_DUAL_ENVIRONMENT_PLAN.md).
