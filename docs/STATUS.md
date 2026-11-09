# Project Status

**Current milestone:** P1-B — W1 multi-file Calculator  
**Release status:** no release has been declared.

## Merged P0 foundation

P0 PR #1 is merged into `main`.

- C++17 CMake project and minimal CLI
- GoogleTest / CTest baseline
- GCC warning and optional ASan/UBSan settings
- clang-format and clang-tidy integration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, roadmap, and decision documents
- eight-week curriculum map

## Merged P1-A — W1 Hello World

P1-A PR #2 is merged into `main`.

- task registry and CLI: `list/start/lesson/hint/check/status/next`
- learner workspace creation with no-overwrite behavior
- separate learner and reference implementations
- fixed GCC compile/run check with exact-output acceptance
- bounded process output and execution time
- process-group termination on timeout
- source fingerprint and stale local evidence detection
- symlink/path-traversal rejection for declared learner source paths
- end-to-end check and stale-evidence regression coverage

Hosted Ubuntu 24.04 CI passed 15/15 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks. Local WSL2 acceptance remains outstanding.

## P1-B — W1 multi-file Calculator

**Current implementation branch:** `feat/p1b-calculator`  
**Status:** implementation and CI validation in progress.

- second registered task: `w1.calculator`
- multi-file learner project: public interface, implementation, and CLI
- separate reference implementation
- platform-owned CMake/GoogleTest fixture
- fixed interface header used by the acceptance build
- seven platform-owned unit tests plus two CLI smoke checks
- learner-provided CMakeLists.txt and tests are not the acceptance authority

The latest hosted Ubuntu 24.04 workflow on commit `982a80686ac4eceb0015df4d8efa750fe4d46976` passed 17/17 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

## Not yet implemented

- cross-workspace prerequisite graph and persistent progress
- task catalog externalization
- workspace recovery and evidence history
- remaining W1–W8 learner/reference task packs
- verified offline dependency package
- first release package

## Remaining acceptance

- Complete P1-B build, test, sanitizer, formatting, and static-analysis checks.
- Run `bash scripts/check_environment.sh --strict-wsl` on the maintainer's Windows 11 + WSL2 + Ubuntu 24.04 machine.
- Exercise both W1 tasks manually in the target environment.

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. Local evidence is editable navigation data, not a tamper-proof certificate. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
