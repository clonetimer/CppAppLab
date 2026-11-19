# W2 · Persistent Todo CLI

## Start here

Build the Todo program in small steps. Do not try to implement persistence and every command at once.

1. Start with an empty list and make `add` append one valid item.
2. Make `list` display items in ID order.
3. Implement `done` and `remove` for an existing ID.
4. Try an unknown ID and an empty title. The program should report an error without changing other items.
5. Add saving and loading only after the in-memory operations work. Run the program twice to prove that data survives between runs.

**What the STL containers do:** `vector` stores an ordered group of items; `string` stores a title; algorithms such as `find_if` search the collection. You do not need to memorize all STL algorithms before starting—use the interface and examples as you implement each operation.

The platform's fixed acceptance checks include malformed files and interrupted/invalid persistence cases. Treat those as later robustness work after the basic Todo loop works.

## From W1 to W2: reuse what you already know

**Mental model:** W2 does not introduce a new way to build a C++ program. It reuses the W1 header/implementation split and the `Result` error-reporting pattern, then adds a collection of records and durable storage.

| You learned in W1 | You will reuse in W2 |
|---|---|
| `calculator.hpp` declares an interface and `calculator.cpp` implements it | `todo.hpp` declares `Item`, `List`, `Result` and operations; `todo.cpp` implements them |
| `Result` carries a value and an error | Todo's `Result` carries an `Error` and a diagnostic message |
| `main.cpp` calls a function implemented in another file | The provided Todo CLI calls `load`, `add`, `mark_done`, `remove` and `save` |
| A fixed platform test checks behavior | The platform-owned tests check both normal operations and failure cases |

The new ideas are introduced one at a time: `std::vector<Item>` stores several records, STL algorithms find them, and file I/O keeps them between separate program runs. You do not need to design a new class hierarchy or write a new command-line parser.

## Recommended implementation order

Follow the existing `TODO(W2-TODO-1...6)` markers in `src/todo.cpp`. The order below makes the starter CLI runnable as early as possible:

1. **Make an empty workspace load successfully.** `main.cpp` calls `load` before every command. If the data file does not exist yet, return success with an empty list. This is the bootstrap case, not a malformed-file error.
2. **Implement `next_id`.** Return 1 for an empty list; otherwise find the largest ID and add one. Return 0 if the ID space is exhausted.
3. **Implement the in-memory operations.** Complete `add`, `mark_done` and `remove`. Test one valid operation and one invalid input before moving on.
4. **Implement save/load round-trip.** Write the version header and escaped records, then load them back. Verify that IDs, completion flags and titles are preserved.
5. **Make loading transactional.** Parse and validate into a temporary list. Replace the caller's list only after the entire file is valid; a malformed later row must not erase existing items.
6. **Run the multi-process CLI sequence.** Add an item in one process, list it in another, mark it done, remove another, then list again. This proves persistence rather than only in-memory behavior.

If you get stuck, keep the compiler feedback narrow: implement one TODO marker, build, and read the first diagnostic before changing another function.

## Goal

Build a small command-line application that manages a list of tasks across separate program runs. This exercise introduces the STL as a practical data-management toolkit and makes file persistence part of the program's behavior.

You will use:

- `std::vector` to own an ordered collection of task records
- `std::string` and `std::string_view` to handle task titles and command input
- STL algorithms such as `std::find_if`, `std::max_element`, and `std::any_of`
- `std::unordered_map` to detect duplicate IDs while loading data
- `std::fstream` and `std::filesystem` to read and write persistent data

## Your task

Implement the functions declared in `include/todo.hpp` in `src/todo.cpp`. The command-line front end in `src/main.cpp` is provided.

The CLI supports four operations:

| Command | Behavior |
|---|---|
| `todo add <title>` | Add a task and assign the next ID |
| `todo list` | Display all tasks in ID order |
| `todo done <id>` | Mark an existing task complete |
| `todo remove <id>` | Remove an existing task |

Use one quoted shell argument for a title containing spaces.

## Data model and interface

The fixed platform interface is:

```cpp
namespace cppapplab::todo {

enum class Error { None, InvalidId, InvalidTitle, DuplicateId, NotFound, InvalidFormat, IoError };

struct Item {
    std::uint64_t id{};
    std::string title;
    bool completed{false};
};

using List = std::vector<Item>;

struct Result {
    Error error{Error::None};
    std::string message;
    bool ok() const noexcept;
};

std::uint64_t next_id(const List& items) noexcept;
Result add(List& items, std::uint64_t id, std::string_view title);
Result mark_done(List& items, std::uint64_t id);
Result remove(List& items, std::uint64_t id);
Result save(const List& items, const std::filesystem::path& path);
Result load(List& items, const std::filesystem::path& path);

}
```

The header in your learner workspace is supplied for editor assistance. The platform acceptance build uses its own fixed copy of the interface. Changing the learner header cannot change the acceptance contract.

## Persistence format

The data file is UTF-8 text with a version header:

```text
CPPAPPLAB_TODO_V1
1<TAB>0<TAB>Review STL containers
2<TAB>1<TAB>Finish the first exercise
```

Each record contains an ID, a completion flag (`0` or `1`), and a title. Fields are separated by tabs. To preserve titles containing control characters, encode backslash, tab, newline, and carriage return as `\\`, `\t`, `\n`, and `\r` respectively.

The loader must reject malformed records, invalid IDs, invalid completion flags, duplicate IDs, and unsupported escape sequences. Parse into a temporary list first; replace the caller's list only after the entire file has been read and validated. A failed load must not destroy the caller's existing in-memory data.

The platform contract limits the file to 1 MiB and each title to 4,096 bytes. Data paths must be regular files rather than symbolic links. Saving should write a temporary file and publish it by rename so an interrupted write does not leave a half-written data file.

## Storage location

By default, the CLI stores data in:

`~/.local/share/cppapplab/todos.tsv`

Set `CPPAPPLAB_TODO_FILE` to select a different path. This is useful for testing and for keeping separate Todo lists.

## Build and run locally

From the learner workspace:

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/todo.cpp src/main.cpp -o build/todo

export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review STL containers"
./build/todo add "Implement file persistence"
./build/todo list
./build/todo done 1
./build/todo remove 2
./build/todo list
```

Each command starts a new process. The next invocation must reload the file and display the state saved by the previous invocation.

## What the platform checks

Run the explicit check from the CppAppLab repository:

```bash
./build/cppapplab check "/absolute/path/to/your/todo-workspace"
```

The fixed acceptance fixture:

1. compiles the declared learner implementation against the platform-owned interface
2. runs 18 GoogleTest cases covering IDs, CRUD behavior, validation, serialization, malformed files, and symlink handling
3. runs an end-to-end CLI workflow across separate process invocations
4. checks that a missing task produces a useful error and a nonzero exit code
5. records the source fingerprint and bounded check log

The learner's own build files and tests are not used as the acceptance authority.

## Explain your design

1. Why is `std::vector` appropriate for this ordered collection? When would a map be a better primary data structure?
2. Why is `std::find_if` preferable to duplicating a hand-written search loop in each operation?
3. Why must `load` parse into a temporary collection before replacing the existing one?
4. What can go wrong if the program writes directly to the destination file and is interrupted halfway through?
5. Why does a version header make future file-format changes easier to manage?

## Troubleshooting

- **Even `list` stops with `not implemented`.** Inspect `load` in `src/todo.cpp`: the supplied `main` calls it before dispatching any command. Complete its missing-file behavior first; a file that does not yet exist must load successfully as an empty list.
- **An added task disappears on the next invocation.** Check that both commands use the same `CPPAPPLAB_TODO_FILE` path, then inspect `save` and the file it wrote. Retry the shown add/list sequence with one test path and verify the version header and saved record.
- **Adding after a removal chooses a duplicate ID.** Inspect `next_id`; the list size is not the largest ID. Try existing IDs 2, 9 and 4: the next ID must be 10. Also check the empty-list and exhausted-ID cases before retrying the CLI.
- **A malformed file replaces some or all existing items.** Inspect where `load` changes its caller's list. Keep parsing and validation in temporary state, and publish only after the whole file succeeds. Retest with one valid record followed by a malformed record and confirm the original list survives.

## Acceptance

- [ ] The next ID is one greater than the maximum existing ID, with overflow handled.
- [ ] Add rejects zero IDs, invalid titles, and duplicate IDs.
- [ ] Mark-done and remove report missing IDs without corrupting the list.
- [ ] Save/load preserve IDs, completion state, and escaped title content.
- [ ] Malformed files and duplicate IDs are rejected without partially replacing the current list.
- [ ] Symbolic-link data paths are rejected.
- [ ] The CLI persists changes across separate invocations.
- [ ] All 18 fixed unit tests and the CLI workflow test pass.
- [ ] The learner source remains unchanged during the acceptance check.
