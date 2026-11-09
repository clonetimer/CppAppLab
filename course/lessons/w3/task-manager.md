# W3 · Task / TaskManager Refactor

## Goal

Refactor the W2 Todo CLI core into an object-oriented design without changing its command-line behavior or on-disk data format. The task focuses on class invariants, encapsulation, RAII, smart-pointer ownership, copy/move semantics, and operator overloading.

You will build two collaborating classes:

- `Task`: a value object that owns its title and exposes a controlled interface
- `TaskManager`: the owner and coordinator of Task objects

## Your task

Implement the declarations in:

- `include/task.hpp`
- `include/task_manager.hpp`

in:

- `src/task.cpp`
- `src/task_manager.cpp`

The command-line front end in `src/main.cpp` is provided. Preserve the W2 commands:

- `todo add <title>`
- `todo list`
- `todo done <id>`
- `todo remove <id>`

The file format remains `CPPAPPLAB_TODO_V1`, so data produced by W2 remains readable by W3.

## Design contract

### Task

`Task` owns its ID, title, and completion state.

- Construction rejects ID zero and empty/whitespace-only titles.
- State is accessed through getters; completion changes through `mark_done()`.
- Copy and move operations use the value semantics of the owned `std::string`.
- `operator==` compares task identity by ID.
- `operator<` orders tasks by ID.
- `operator<<` formats a task as `#id [x] title`.

### TaskManager

`TaskManager` owns tasks using:

```cpp
std::vector<std::unique_ptr<Task>> tasks_;
```

The owning container must never expose ownership to callers. `find()` returns a non-owning pointer, while `snapshot()` returns value copies sorted by ID.

Implement:

- Add, find, mark-done, remove, next-ID, size, empty, and snapshot operations.
- A deep-copy constructor that creates independent `Task` objects.
- Copy assignment with self-assignment safety and a strong exception-safety strategy such as copy-and-swap.
- Move construction and move assignment that transfer ownership and leave the source manager empty and valid.
- `swap()` for exchanging owned collections.

Removing a task must release its object automatically through `std::unique_ptr`. Do not use raw owning pointers or manual `new` / `delete`.

## Persistence compatibility

Keep the W2 data format and storage behavior:

- Version header: `CPPAPPLAB_TODO_V1`
- Record fields: ID, completion flag, escaped title
- Escapes: backslash, tab, newline, carriage return
- Maximum file size: 1 MiB
- Maximum title size: 4,096 bytes
- Reject malformed records, duplicate IDs, and symbolic-link paths
- Load into a temporary `TaskManager` and replace the current state only after complete validation
- Save through a temporary sibling file and rename

The default file remains `~/.local/share/cppapplab/todos.tsv`; `CPPAPPLAB_TODO_FILE` selects an alternate file.

## Build locally

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/task.cpp src/task_manager.cpp src/main.cpp -o build/todo

export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review unique ownership"
./build/todo add "Verify copy and move"
./build/todo list
./build/todo done 1
./build/todo remove 2
```

## What the platform checks

Run:

```bash
./build/cppapplab check "/absolute/path/to/your/task-manager-workspace"
```

The fixed acceptance fixture runs 30 GoogleTest cases plus one CLI workflow test. It checks value invariants, identity/order operators, deep copy, copy assignment, self-assignment, move construction, move assignment, snapshot ordering, W2 file compatibility, malformed data, and transactional loading. The CLI test verifies that the refactored classes preserve the W2 command behavior across separate process invocations.

The platform compiles your implementation against its own copies of both headers. Learner-controlled CMake files and tests are not the acceptance authority.

## Explain your design

1. Why does `TaskManager` own `std::unique_ptr<Task>` instead of exposing raw owning pointers?
2. Why must copying a manager create new `Task` objects rather than copy the `unique_ptr` values?
3. How does copy-and-swap protect the target object if allocation fails during copying?
4. What state should a moved-from manager guarantee to callers?
5. Why is task identity based on ID, while the title and completion state remain mutable values?
6. How does preserving the W2 file format make this a refactor rather than a replacement application?

## Acceptance

- [ ] Task construction enforces its invariants.
- [ ] Task equality and ordering use stable ID identity.
- [ ] TaskManager uses unique ownership and releases removed tasks through RAII.
- [ ] Copy construction and assignment produce independent task objects.
- [ ] Move construction and assignment transfer ownership and leave the source empty.
- [ ] Snapshot returns ordered value copies.
- [ ] W2 data files round-trip without format changes.
- [ ] Malformed loads do not partially replace manager state.
- [ ] The CLI retains add/list/done/remove behavior.
- [ ] All 30 unit tests and the CLI workflow test pass.
