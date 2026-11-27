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

**Cross-platform file safety / 跨平台文件安全：** the starter provides `unsafe_data_link(path)` for the existing leaf-path contract. On Linux it uses the standard filesystem symlink check; on native Windows it checks the file's reparse-point attributes using the OS API. Call it before reading the data file and before writing/replacing either the destination or the temporary sibling; return `Error::IoError` for unsafe paths. Missing files are handled by the original load/create logic. You still implement the list/ownership, serialization, transactional load and atomic publication yourself. This helper does not replace the platform's wider workspace confinement. Never bypass a failed link-safety test. On Windows, real file-symlink acceptance additionally needs Developer Mode or existing symlink-creation permission.

Windows 用户：使用已有的 `unsafe_data_link` 辅助函数检查数据文件与临时文件，遇到重解析点或无法安全检查的路径应返回 `IoError`。课程算法、文件格式和原有验收标准不变。
