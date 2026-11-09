# Project Status

**Current milestone:** P0 — Foundation  
**Release status:** no release has been declared.

## Implemented in the initial foundation branch

- Repository initialized with project scope and first-version environment
- C++17 CMake project and minimal \`cppapplab\` CLI
- GoogleTest / CTest smoke tests
- GCC warning and optional ASan/UBSan settings
- optional clang-tidy integration and clang-format configuration
- WSL2/Ubuntu toolchain checker
- GitHub Actions build/test, sanitizer, and quality workflows
- architecture, development, environment, learning contract, and roadmap documents
- eight-week curriculum map

## Not yet implemented

- learning-engine commands and persistent progress
- task catalog parser and prerequisite graph
- learner workspace generator
- fixed acceptance harness and source fingerprinting
- actual W1–W8 lesson projects and learner/reference pairs
- verified offline dependency package
- first release package

## Evidence policy

A repository commit is not evidence that the code passed. P0 is complete only after the branch CI has run successfully and the build/test commands have been checked in a clean Ubuntu 24.04 environment. Any local WSL2-specific result must be recorded separately from GitHub-hosted Linux CI.
