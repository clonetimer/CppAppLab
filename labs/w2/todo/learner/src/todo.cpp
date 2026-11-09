#include "todo.hpp"

namespace cppapplab::todo {

std::uint64_t next_id(const List& items) noexcept {
    // TODO(W2-TODO-1): use an STL algorithm to find the next available ID.
    (void)items;
    return 1U;
}

Result add(List& items, std::uint64_t id, std::string_view title) {
    // TODO(W2-TODO-2): validate the ID/title and append a new item.
    (void)items;
    (void)id;
    (void)title;
    return {Error::InvalidTitle, "not implemented"};
}

Result mark_done(List& items, std::uint64_t id) {
    // TODO(W2-TODO-3): find an item by ID and mark it completed.
    (void)items;
    (void)id;
    return {Error::NotFound, "not implemented"};
}

Result remove(List& items, std::uint64_t id) {
    // TODO(W2-TODO-4): use an STL algorithm to remove an item by ID.
    (void)items;
    (void)id;
    return {Error::NotFound, "not implemented"};
}

Result save(const List& items, const std::filesystem::path& path) {
    // TODO(W2-TODO-5): serialize the list and persist it to disk.
    (void)items;
    (void)path;
    return {Error::IoError, "not implemented"};
}

Result load(List& items, const std::filesystem::path& path) {
    // TODO(W2-TODO-6): parse the file into a temporary list, then replace items.
    (void)items;
    (void)path;
    return {Error::InvalidFormat, "not implemented"};
}

}  // namespace cppapplab::todo
