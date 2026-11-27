## Hint 1

Start by treating the Todo list as a collection of records. `List` is already an alias for `std::vector<Item>`. Use the vector to preserve insertion order. Look at the signatures: `add`, `mark_done`, and `remove` should each perform one small operation and return a `Result`.

## Hint 2

Use standard algorithms rather than writing a different search loop for every operation:

- `std::max_element` can find the largest ID.
- `std::find_if` can locate an item by ID.
- `std::any_of` can determine whether a title contains a non-whitespace character.
- `std::unordered_map` can track IDs already seen during loading.

For file loading, parse each line into a separate candidate vector. Do not modify the caller's vector while the file is still being validated.

## Hint 3

A robust implementation can be divided into four layers:

1. Validate IDs and titles before changing the list.
2. Use STL algorithms to find, add, update, and erase records.
3. Serialize one record per line, escaping `\\`, tabs, newlines, and carriage returns. Check the version header and every field while loading.
4. Write the complete serialized content to a temporary sibling file, then rename it to the requested path. On load failure, return an error and leave the original in-memory list unchanged.

Test the parser with empty titles, duplicate IDs, invalid completion flags, truncated escape sequences, and a file with the wrong version header.

**Cross-platform file safety / 跨平台文件安全：** the starter provides `unsafe_data_link(path)` for the existing leaf-path contract. On Linux it uses the standard filesystem symlink check; on native Windows it checks the file's reparse-point attributes using the OS API. Call it before reading the data file and before writing/replacing either the destination or the temporary sibling; return `Error::IoError` for unsafe paths. Missing files are handled by the original load/create logic. You still implement the list/ownership, serialization, transactional load and atomic publication yourself. This helper does not replace the platform's wider workspace confinement. Never bypass a failed link-safety test. On Windows, real file-symlink acceptance additionally needs Developer Mode or existing symlink-creation permission.

Windows 用户：使用已有的 `unsafe_data_link` 辅助函数检查数据文件与临时文件，遇到重解析点或无法安全检查的路径应返回 `IoError`。课程算法、文件格式和原有验收标准不变。
