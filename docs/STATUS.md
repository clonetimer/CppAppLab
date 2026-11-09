# Project Status

**Current milestone:** P1-A — W1 Hello World learning-engine vertical slice  
**Release status:** no release has been declared.

## Merged P0 foundation

- C++17 CMake project and minimal CLI
- GoogleTest / CTest smoke tests
- GCC warning and optional ASan/UBSan settings
- optional clang-tidy integration and clang-format configuration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, roadmap, and decision documents
- eight-week curriculum map

P0 PR #1 was merged after GCC build, GoogleTest, ASan/UBSan, clang-format, and clang-tidy jobs passed. The local WSL2 acceptance check remains a separate maintainer-machine verification item.

## P1-A implementation in progress

- task registry for W1 Hello World
- learner workspace creation that refuses to overwrite existing directories
- separate reference implementation
- lesson and three progressive hints
- explicit GCC compile/run check with exact-output contract
- bounded process output and execution time
- source fingerprint and stale local evidence detection
- CLI: list/start/lesson/hint/check/status/next

P1-A is not complete until the new engine code and tests pass CI. The local evidence file is a navigation record, not a tamper-proof certificate.

## Not yet implemented

- W1 Calculator multi-file acceptance fixture
- cross-workspace prerequisite graph and persistent progress
- remaining W1–W8 learner/reference task packs
- full acceptance harness for CMake/GoogleTest projects
- verified offline dependency package
- first release package

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
