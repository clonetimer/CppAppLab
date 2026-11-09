#pragma once

#include "http_message.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace cppapplab::http {

enum class ParseStatus {
    NeedMoreData,
    Complete,
    Error
};

struct ParseResult {
    ParseStatus status{ParseStatus::NeedMoreData};
    HttpRequest request;
    std::size_t consumed{0U};
    std::string error;
};

class HttpRequestParser {
   public:
    [[nodiscard]] ParseResult parse(std::string_view buffer) const;
};

}  // namespace cppapplab::http
