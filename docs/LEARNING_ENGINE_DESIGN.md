# Learning Engine — P1/P2 Design

## Current learning loop

The available task sequence begins with W1 Hello World and continues to the W1 multi-file Calculator:

```text
list
  -> start
  -> lesson
  -> hint 1 / 2 / 3
  -> edit learner source
  -> check
  -> status
  -> next
```

Only explicit `check` compiles and executes learner code. `list`, `lesson`, `hint`, `status`, `next`, `progress`, `resume`, and `history` are navigation/read-only operations.

## Workspace boundary

- A task starts by copying only its learner template.
- Reference source remains in the repository's reference tree and is not copied to the learner workspace.
- The destination directory is reserved before copying; an existing destination is never overwritten.
- Template symlinks and unsupported file types are rejected.
- The workspace marker identifies the task and platform version.
- Build output and local check records are stored under `.cppapplab/` inside the learner workspace.

## W1 Hello World check contract

- source file: `src/main.cpp`
- compiler: GCC with `-std=c++17 -Wall -Wextra -Wpedantic -Werror`
- maximum compile time: 30 seconds
- maximum program time: 5 seconds
- combined stdout/stderr capture is bounded
- stdin is redirected from `/dev/null`
- timeout kills the process group
- expected stdout: exactly `Hello, CppAppLab!\n`
- process exit code: 0

## W1 Calculator check contract

The calculator task is built through a platform-owned CMake fixture, not the learner's CMakeLists.txt.

- learner source files are fingerprinted before and after checking
- only declared `.cpp` implementation files are staged into a controlled build directory
- the fixture supplies its own fixed `calculator.hpp` interface
- the fixture builds the learner library and CLI with C++17 and warnings-as-errors
- seven platform-owned GoogleTest cases cover four operations, division by zero, and negative/zero operands
- two CLI smoke checks cover a normal calculation and division by zero
- the learner's CMakeLists.txt, tests, and copied header cannot replace the platform acceptance contract
- configure, build, CTest, and CLI execution all have bounded time/output

The calculator task records 9 acceptance checks: seven fixed unit tests and two CLI smoke checks.

## Course catalog and prerequisites

Task metadata is externalized in `course/tasks.tsv`. Each row declares the task ID, display title, week/order, lesson and hint files, learner template, source files, acceptance type, fixed acceptance metadata, and prerequisite task IDs.

The loader validates the catalog before exposing tasks: task IDs must be unique and well-formed; paths must be relative and cannot contain `.` or `..`; acceptance metadata must match its acceptance type; prerequisites must exist and appear earlier in course order.

The Calculator task depends on `w1.hello-world`. The `start` command refuses to create a dependent workspace until the prerequisite has a current PASS.

## Persistent progress and recovery

Progress is stored outside the repository by default:

```text
~/.local/share/cppapplab/progress/
  w1.hello-world.progress
  w1.calculator.progress
  history/
    w1.hello-world.history
    w1.calculator.history
```

Set `CPPAPPLAB_STATE_DIR` to select another state root. Starting a workspace writes a `STARTED` record before the learner runs a check. Each explicit check updates the latest task record and appends a history entry. History is bounded to 1 MiB per task.

- `cppapplab progress`: show NOT STARTED, IN PROGRESS, LOCKED, PASSED, NEEDS WORK, and STALE states
- `cppapplab resume <task-id>`: reopen the most recently started workspace, even before the first check
- `cppapplab history <task-id>`: inspect previous check attempts

The latest workspace pointer is maintained per task, while the check history retains attempts from multiple workspaces. A prerequisite can be satisfied by any workspace whose latest recorded attempt is PASS, whose workspace still exists, whose local evidence agrees, and whose current declared-source fingerprint still matches. Starting a second workspace does not erase a valid PASS in another workspace. A later FAIL in the same workspace supersedes its previous PASS.

These files are local navigation state and remain editable by the user; they are not tamper-proof certificates.

## W2 Todo CLI check contract

The W2 task uses the same platform-owned CMake acceptance mechanism as the Calculator, with a task-specific fixture.

- task ID: `w2.todo-cli`
- learner implementation files: `src/todo.cpp` and `src/main.cpp`
- the platform supplies a fixed `todo.hpp` interface to the acceptance build
- 18 GoogleTest cases cover ID allocation, validation, add/update/remove, escaped text round-trips, malformed files, duplicate IDs, and symbolic-link rejection
- one CTest integration case executes add/list/done/remove across separate CLI process invocations using an isolated `CPPAPPLAB_TODO_FILE`
- expected CTest count: 19
- task prerequisite: a current PASS for `w1.calculator`

The persistence format is versioned with a `CPPAPPLAB_TODO_V1` header. A record contains ID, completion flag, and escaped title. Loading parses into a temporary collection and replaces the current collection only after complete validation. Saving writes a temporary sibling file and publishes it by rename. The default data file is `~/.local/share/cppapplab/todos.tsv`; `CPPAPPLAB_TODO_FILE` overrides it for isolated workspaces and tests.


## W3 Task / TaskManager check contract

Task ID: `w3.task-manager`; prerequisite: a current PASS for `w2.todo-cli`.

The learner implements:

- `src/task.cpp`
- `src/task_manager.cpp`
- the supplied CLI entry point remains `src/main.cpp`

The fixed platform interface separates the `Task` value object from the owning `TaskManager`. The manager owns a `std::vector<std::unique_ptr<Task>>`; callers receive non-owning pointers from `find()` and value copies from `snapshot()`.

The acceptance fixture supplies platform-owned copies of both headers and runs 30 GoogleTest cases plus one multi-process CLI workflow test (31 total). Tests cover constructor invariants, ID identity/order, stream output, invalid input, CRUD, overflow, const lookup, deep copy, copy-and-swap assignment, self-assignment, move semantics, ordered snapshots, W2-format compatibility, escaped fields, malformed/duplicate records, and symlink rejection.

Persistence deliberately retains the W2 `CPPAPPLAB_TODO_V1` format. Loading parses into a temporary manager and swaps only after complete validation. Saving writes a temporary sibling file and publishes it by rename. This makes W3 an internal ownership refactor rather than a data migration or replacement application.

The root integration test copies the reference implementation into a temporary learner workspace and invokes the actual platform check command. It also places a deliberately failing learner `CMakeLists.txt` in that workspace to verify the acceptance runner uses the platform-owned fixture.

## W4 TaskManager quality-gate contract

Task ID: `w4.quality-gate`; prerequisite: a current PASS for `w3.task-manager`.

The learner receives the completed W3 TaskManager implementation and writes ten GoogleTest cases in `tests/task_manager_test.cpp`. The learner's CMake project builds a reusable core library, the CLI, and a separate test executable. A quality script performs a Debug build, CTest, ASan/UBSan, clang-format, clang-tidy-enabled compilation, and optional Valgrind.

The fixed platform fixture supplies its own headers and CMake project. It runs 30 platform-owned behavior tests, compiles and runs the ten learner-authored tests, and executes one multi-process CLI workflow test (41 total). The root integration test invokes the actual `cppapplab check` command and verifies that learner-controlled CMake configuration is not the acceptance authority.

The lesson distinguishes test evidence from static-analysis diagnostics: a passing test suite does not imply zero clang-tidy warnings, and a clean sanitizer run covers only the executed test paths. GDB and Valgrind are taught as reproducible diagnostic tools; Valgrind is optional when unavailable in the target environment.

## W5 Linux system programming contract

Task ID: `w5.system-lab`; prerequisite: a current PASS for `w4.quality-gate`.

The learner implements a bounded producer/consumer queue and a POSIX child-process runner. The queue uses a mutex, condition variables, a deque, predicate waits and an explicit close/drain lifecycle. The process runner uses pipe/fork/dup2/execvp/poll/waitpid, captures stdout and stderr into a bounded buffer, and kills/reaps a child after its deadline.

The platform fixture runs 20 platform-owned behavior tests, ten learner-authored tests and one CLI smoke test (31 total). Tests cover queue capacity/FIFO/close/wakeup/multiple producers-consumers, process output and exit status, argv safety, argument validation, output bounds, timeout termination, and descriptor write completion.

The task is intentionally Linux/POSIX-specific and is accepted on WSL2 Ubuntu 24.04. It does not claim Windows-native compatibility. The CLI passes an argument vector directly; it does not implicitly interpret a shell command string.

## Evidence semantics

The local evidence and log are navigation records, not certificates. They are user-editable files and must never be represented as tamper-proof proof of learning.

The source fingerprint uses FNV-1a 64-bit to detect ordinary source changes. It is not a cryptographic integrity mechanism. Declared source paths are checked for path traversal and symlink components. A changed fingerprint marks the recorded result stale; learners must rerun the explicit check.

A passing check proves only that the declared source, at that moment, compiled and passed the fixed behavior contract. It does not prove conceptual understanding or provide a security boundary.

## Execution boundary

The process runner uses direct `execvp` argument vectors (no shell interpolation), captures stdout/stderr, redirects stdin from `/dev/null`, enforces a time limit, bounds captured output, and kills the process group on timeout. This is operational control, not a hostile-code sandbox. Learner programs still run with the learner's operating-system permissions.

## Next vertical slice

P1-C now provides external task metadata, prerequisite enforcement, persistent progress, workspace recovery, and per-task check history. The next course work product after W3 is W4 engineering quality: test design, debugging, sanitizers, static analysis, and reproducible builds.
