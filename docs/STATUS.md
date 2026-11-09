# Project Status

**Current milestone:** P0 — Foundation  
**Release status:** no release has been declared.

## Implemented in the initial foundation branch

- Repository initialized with project scope and first-version environment
- C++17 CMake project and minimal `cppapplab` CLI
- GoogleTest / CTest smoke tests
- GCC warning and optional ASan/UBSan settings
- optional clang-tidy integration and clang-format configuration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, and roadmap documents
- eight-week curriculum map

## Latest automated evidence — 2026-11-09

Commit: `2c3be9e5ed3a92c9272d4581cee69c6d20d5bb02`

- GCC Build and GoogleTest: PASS, 2/2 tests
- GCC ASan and UBSan: PASS, 2/2 tests
- clang-format check: PASS
- clang-tidy-enabled build and tests: PASS
- Runner: GitHub-hosted Ubuntu 24.04

The first formatting run failed because clang-format reordered the project header include blocks. The configuration was corrected to preserve include order; the subsequent push and pull-request workflows both passed.

## Remaining P0 verification

- Local WSL2 + Ubuntu 24.04 environment check has not yet been run on the maintainer's Windows machine.
- The environment checker and full local acceptance commands are documented in [the WSL2 guide](ENVIRONMENT_WSL2_UBUNTU.md).

## Not yet implemented

- learning-engine commands and persistent progress
- task catalog parser and prerequisite graph
- learner workspace generator
- fixed acceptance harness and source fingerprinting
- actual W1–W8 lesson projects and learner/reference pairs
- verified offline dependency package
- first release package

## Evidence policy

A repository commit is not evidence that the code passed. Hosted Ubuntu CI and local WSL2 acceptance are recorded separately. A release is not declared until the target WSL2 environment and the required course workflow have been verified.
