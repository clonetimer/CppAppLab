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
