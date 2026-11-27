#include "todo.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cppapplab::todo {

namespace {

// The data-file contract rejects links before read or replacement. MinGW's
// filesystem::symlink_status can follow Windows links in older libstdc++.
// This leaf check is not a sandbox or a general directory-confinement check.
[[maybe_unused, nodiscard]] bool unsafe_data_link(const std::filesystem::path& path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        // Missing files are allowed; other inspection failures are unsafe.
        return error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND;
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    return std::filesystem::is_symlink(path);
#endif
}

}  // namespace

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
    // Use unsafe_data_link(path) and unsafe_data_link(temporary) before writing.
    (void)items;
    (void)path;
    return {Error::IoError, "not implemented"};
}

Result load(List& items, const std::filesystem::path& path) {
    // TODO(W2-TODO-6): parse the file into a temporary list, then replace items.
    // Reject unsafe_data_link(path) before inspecting or opening the file.
    (void)items;
    (void)path;
    return {Error::InvalidFormat, "not implemented"};
}

}  // namespace cppapplab::todo
