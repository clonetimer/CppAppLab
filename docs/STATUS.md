# Project Status

**Current milestone:** P1-A — W1 Hello World learning-engine vertical slice  
**Release status:** no release has been declared.

## Merged P0 foundation

- C++17 CMake project and minimal CLI
- GoogleTest / CTest baseline
- GCC warning and optional ASan/UBSan settings
- clang-format and clang-tidy integration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, roadmap, and decision documents
- eight-week curriculum map

P0 PR #1 is merged into \`main\`. Its GCC build, GoogleTest, ASan/UBSan, clang-format, and clang-tidy checks passed. Local WSL2 acceptance remains a separate maintainer-machine verification item.

## P1-A implementation

Implemented on \`feat/p1-learning-engine\`; PR #2 is awaiting review and merge.

- task registry for W1 Hello World
- learner workspace creation that reserves the destination and refuses to overwrite existing work
- separate learner and reference implementations
- lesson and three progressive hints
- explicit GCC compile/run check with a fixed exact-output contract
- bounded process output and execution time
- process-group termination on timeout
- source fingerprint and stale local evidence detection
- rejection of symlinked or path-traversing learner source paths
- CLI: \`list\`, \`start\`, \`lesson\`, \`hint\`, \`check\`, \`status\`, \`next\`

Only the explicit \`check\` command compiles and executes learner code. Navigation commands remain read-only.

## Latest automated evidence — 2026-11-09

Verified commit: \`b0405f8b5db04aeaa96ecf290afac9a1a7bb3621\`

- GCC Build and GoogleTest: PASS, 15/15 tests
- GCC ASan and UBSan: PASS, 15/15 tests
- clang-format: PASS
- clang-tidy-enabled build and tests: PASS
- Runner: GitHub-hosted Ubuntu 24.04

The end-to-end tests create a learner workspace, compile and run the learner's actual source against the fixed contract, verify the recorded result, then modify the source and confirm that the previous result becomes stale.

The local evidence file remains editable navigation data, not a tamper-proof certificate.

## Not yet implemented

- W1 Calculator multi-file acceptance fixture
- cross-workspace prerequisite graph and persistent progress
- task catalog externalization
- workspace recovery and evidence history
- remaining W1–W8 learner/reference task packs
- verified offline dependency package
- first release package

## Remaining acceptance

- Run \`bash scripts/check_environment.sh --strict-wsl\` on the maintainer's Windows 11 + WSL2 + Ubuntu 24.04 machine.
- Complete the PR #2 review and merge after the final branch checks pass.
- Exercise the workflow manually in the target environment.

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
