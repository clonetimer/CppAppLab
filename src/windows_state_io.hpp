#pragma once

#ifndef _WIN32
#error "windows_state_io.hpp is only for native Windows builds"
#endif

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace cppapplab::windows_state_io {

// CP7 native state storage is restricted to local fixed-drive paths. No
// reparse-point traversal, device paths, alternate streams or hard-linked files.
[[nodiscard]] std::optional<std::filesystem::path> environment_path(const wchar_t* name);
void ensure_directory(const std::filesystem::path& directory);
[[nodiscard]] bool regular_exists(const std::filesystem::path& path);
[[nodiscard]] std::string read(const std::filesystem::path& path, std::uintmax_t maximum_bytes);
void atomic_write(const std::filesystem::path& path, std::string_view content);
void append(const std::filesystem::path& path, std::string_view line,
            std::uintmax_t maximum_bytes);

}  // namespace cppapplab::windows_state_io
