## Hint 1

Keep the responsibilities separate. `Task` owns one record and protects its invariants. `TaskManager` owns the collection and controls how records enter, leave, and move between managers.

The member `std::vector<std::unique_ptr<Task>>` means each task has one owner. A caller that needs to inspect a task should receive a non-owning pointer or a value snapshot, not ownership of the pointer.

## Hint 2

For deep copy, reserve space and create a new `Task` for every object in the source manager. For copy assignment, first construct a complete temporary copy, then swap its collection with the target. This avoids leaving the target half-copied if allocation fails.

For moves, transfer the vector of `unique_ptr` objects and explicitly clear the source collection so the moved-from manager has a predictable empty state.

## Hint 3

A useful implementation order is:

1. Implement `Task` construction, accessors, `mark_done()`, and the three operators.
2. Implement TaskManager lookup, add, remove, next-ID, and snapshot.
3. Implement copy construction and copy assignment using deep copies and copy-and-swap.
4. Implement move construction and move assignment with ownership transfer.
5. Reuse the W2 file format, but parse into a temporary manager before swapping it into place.
6. Keep the CLI thin: it should call TaskManager operations rather than manipulate ownership directly.
