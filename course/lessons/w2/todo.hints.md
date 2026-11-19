## Hint 1

Start from the W1 pattern: the header defines the contract and `todo.cpp` implements it. The provided `main.cpp` calls `load` before it runs any command, so first make a missing data file mean “an empty list” and return success. Then implement `next_id` and the small in-memory operations. `List` is already an alias for `std::vector<Item>`; you do not need to design the container type.

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
