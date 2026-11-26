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

## Independent STL and value-type practice

Optional, independent C++17 knowledge experiments. Open **查知识** in the workbench, search the identifier or term, predict the result, run the fixed failure and repair, then explain and try a separate transfer. These probes do not change the original project or formal acceptance. Full fixed sources and actual outputs are in the knowledge panel.

- **lambda-capture** — A lambda with an empty capture list cannot use an outer automatic variable. Choose value/reference capture deliberately. Predict why the empty capture fails.
- **fixed-array** — The size N is part of std::array<T,N>'s type; it cannot accept more initializers than N. Predict the result of two initializers for one element.
- **map-lookup** — map::operator[] inserts a value for a missing key. find can inspect without modifying the map. Predict the collection size after a missing-key lookup.
- **optional** — optional represents absence explicitly; value() throws on empty, value_or gives a chosen fallback. Predict empty value() and then compare value_or.
- **variant** — variant stores one alternative; get<T> throws for a different active type; get_if can be checked for null. Predict get<int> while string is active.
- **string-view** — A string_view borrows storage; it is not a value snapshot. Observe defined mutation while the owner remains alive, then copy to std::string. Never dereference a dangling view.
- **iterator-invalidation** — Force real vector growth and detect invalidation before any invalid access. Keep a logical index across reserve and reacquire an iterator; insert/erase can change index identity.
- **lookup-complexity** — Count actual comparisons on 64 sorted values; the local seven-comparison budget illustrates this case, not universal runtime performance. Explain the sorted-range precondition.

## Modern C++ data and library practice

Optional CP4 examples use a distinct GCC 13 profile, not the existing C++17 assignment. Search the knowledge panel by ID. Predict, execute the actual failure/repair pair, explain the observation, then attempt a separate local implementation. A missing toolchain/feature is not an expected failure. The original fixed course acceptance is unchanged.

- **cpp20-ranges (C++20)** — Compose a lazy filter and observe selected values while preserving the source range.
- **cpp20-span (C++20)** — Compare static extents; keep borrowed storage alive.
- **cpp23-expected (C++23)** — Handle value/error states and catch bad_expected_access.
- **cpp23-byteswap (C++23)** — Decode a fixed-width integer under an explicit byte-order contract.
- **cpp23-contains (C++23)** — Compare position-returning search with Boolean containment.
- **cpp23-underlying (C++23)** — Convert a scoped enum at an explicit numeric boundary.
