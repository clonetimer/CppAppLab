# Learning and Acceptance Contract

This contract defines how future CppAppLab lessons and projects must be authored.

## 1. Three separate artifacts

Every cumulative exercise must distinguish:

1. **Reference implementation:** complete maintainer-owned source used for explanation and regression.
2. **Learner workspace:** independent source tree containing the learner's implementation tasks.
3. **Acceptance fixtures:** platform-owned tests and build instructions that the learner cannot replace by editing their own project.

## 2. Task card schema

A task card should state:

- ID and title
- Prerequisites
- Learning outcomes
- Problem statement and expected behavior
- Files the learner may edit
- Public interfaces that must remain stable
- Constraints and prohibited shortcuts
- Build and check commands
- Acceptance cases: normal, boundary, and error paths
- Progressive hints (conceptual hint, design hint, implementation hint)
- Expected artifact and explanation questions

## 3. Check semantics

A check must perform a fresh configure/build as needed and run the platform-owned tests against the current learner source. It must report:

- toolchain and build configuration
- compile/configure status
- test cases discovered and executed
- passed and failed test counts
- first actionable failure and relevant output
- execution timeout or output-limit status
- source fingerprint and timestamp

A zero-test run, stale report, ignored-only test suite, or learner-edited test contract cannot produce a pass.

## 4. Progress semantics

- Progress is attached to a specific project workspace and task ID.
- A pass is valid only for the source fingerprint that was checked.
- Any relevant source change marks the previous pass as historical/stale.
- Hints are available on demand and do not change task status.
- Completing all guided tasks does not automatically certify independent design ability.
- A separate capstone assesses requirement decomposition, architecture, tests, debugging, and delivery.

## 5. Execution boundary

The first implementation is local and is not a security sandbox. Future execution limits should bound time and captured output, but documentation must not imply that these controls isolate malicious code or protect host credentials.
