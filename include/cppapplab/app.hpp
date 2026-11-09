#pragma once

#include <string_view>

namespace cppapplab {

[[nodiscard]] std::string_view version() noexcept;
[[nodiscard]] std::string_view usage() noexcept;

}  // namespace cppapplab
