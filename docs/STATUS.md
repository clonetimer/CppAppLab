# Project Status

**Current milestone:** P2-B — W3 Task / TaskManager Refactor  
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

Hosted Ubuntu 24.04 CI passed 15/15 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

## Merged P1-B — W1 multi-file Calculator

P1-B PR #3 is merged into `main`.

- second registered task: `w1.calculator`
- multi-file learner project and separate reference implementation
- platform-owned CMake/GoogleTest fixture
- seven fixed unit tests plus two CLI smoke checks
- learner CMakeLists.txt and tests are not the acceptance authority

Hosted Ubuntu 24.04 CI passed 17/17 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks.

## P1-C — persistent progress and workspace recovery

**Branch:** `feat/p1c-progress-state`  
**Status:** merged into `main`; PR #4 is closed.

- task catalog externalized to `course/tasks.tsv`, with validation for paths, acceptance metadata, duplicate IDs, and prerequisite ordering
- explicit prerequisite metadata; Calculator is locked until Hello World has a current PASS
- course progress persisted outside the source checkout
- `progress` displays NOT STARTED, IN PROGRESS, LOCKED, PASSED, NEEDS WORK, and STALE states
- `resume <task-id>` restores the latest saved workspace, including before the first check
- `history <task-id>` displays previous check attempts
- every check records the latest result and appends a bounded history record
- prerequisite PASS can be satisfied by any workspace whose latest attempt is PASS and whose current source fingerprint still matches
- starting a second workspace does not erase a valid PASS from another workspace

Default state location:

`~/.local/share/cppapplab/progress/`

Override with `CPPAPPLAB_STATE_DIR`.

Hosted Ubuntu 24.04 verification on the P1-C branch passed 28/28 tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks. PR #4 was merged as `ff137ea16fed8293dca6ca7e8972d83c72a265e4`. The environment checker reported zero failures and one expected WSL2 warning on the GitHub-hosted runner.

## P2-A — W2 persistent Todo CLI

**Branch:** `feat/p2-w2-todo-cli`  
**Status:** merged into `main` via PR #5.

- task `w2.todo-cli`, dependent on a current W1 Calculator PASS
- STL-backed Todo model using `std::vector`, algorithms, and `std::unordered_map`
- add/list/done/remove command-line operations
- versioned text persistence with escaped titles
- transactional load behavior and atomic save publication
- platform-owned interface and 18 unit tests plus one multi-process CLI workflow test

Hosted Ubuntu 24.04 CI on commit `6e7b3f3b9f5c68f24e832afe33af3e9d0727ad5f` passed 30/30 root tests in both ordinary GCC and ASan/UBSan builds, plus clang-format and clang-tidy-enabled checks. The W2 end-to-end acceptance test independently ran the 19-case Todo fixture and passed.

## P2-B — W3 Task / TaskManager Refactor

**Branch:** `feat/p2-w3-task-manager`  
**Status:** implementation in progress; hosted CI pending.

- `Task` value object with constructor invariants and ID-based equality/order
- `TaskManager` owns tasks through `std::unique_ptr<Task>`
- deep-copy constructor, copy-and-swap assignment, move construction/assignment
- non-owning lookup and ordered value snapshots
- W2-compatible `CPPAPPLAB_TODO_V1` persistence with transactional load and atomic save
- unchanged add/list/done/remove CLI behavior
- platform-owned acceptance fixture: 30 GoogleTest cases plus one CLI workflow test (31 total)
- W3 depends on a current W2 Todo CLI PASS

## Remaining work

- complete W3 hosted GCC, ASan/UBSan, clang-format, and clang-tidy CI
- merge W3 after acceptance passes
- additional W1–W8 learner/reference task packs
- richer task-specific diagnostics and feedback
- verified offline dependency package
- local Windows 11 + WSL2 + Ubuntu 24.04 acceptance
- first release package and release notes

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. Local progress and evidence files are editable navigation data, not tamper-proof certificates. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
