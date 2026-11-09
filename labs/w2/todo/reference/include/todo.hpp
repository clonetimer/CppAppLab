#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

    [[nodiscard]] bool ok() const noexcept {
        return error == Error::None;
    }

    explicit operator bool() const noexcept {
        return ok();
    }
};

[[nodiscard]] std::uint64_t next_id(const List& items) noexcept;

[[nodiscard]] Result add(List& items, std::uint64_t id, std::string_view title);

[[nodiscard]] Result mark_done(List& items, std::uint64_t id);

[[nodiscard]] Result remove(List& items, std::uint64_t id);

[[nodiscard]] Result save(const List& items, const std::filesystem::path& path);

[[nodiscard]] Result load(List& items, const std::filesystem::path& path);

}  // namespace cppapplab::todo
