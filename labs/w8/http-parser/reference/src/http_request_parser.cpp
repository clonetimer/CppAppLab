#include "http_request_parser.hpp"

#include <charconv>
#include <cctype>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace cppapplab::http {
namespace {

constexpr std::size_t kMaxHeaderSize = 16U * 1024U;
constexpr std::size_t kMaxBodySize = 1024U * 1024U;
constexpr std::string_view kTokenPunctuation = "!#$%&'*+-.^_`|~";

[[nodiscard]] bool is_token_character(unsigned char character) {
    const bool alpha_numeric = (character >= 'a' && character <= 'z') ||
                               (character >= 'A' && character <= 'Z') ||
                               (character >= '0' && character <= '9');
    return alpha_numeric ||
           kTokenPunctuation.find(static_cast<char>(character)) != std::string_view::npos;
}

[[nodiscard]] bool valid_method(std::string_view method) {
    return !method.empty() &&
           std::all_of(method.begin(), method.end(), [](unsigned char character) {
               return is_token_character(character);
           });
}

[[nodiscard]] bool valid_target(std::string_view target) {
    return !target.empty() &&
           std::all_of(target.begin(), target.end(), [](unsigned char character) {
               return character > 0x20U && character < 0x7fU;
           });
}

[[nodiscard]] std::string trim_ows(std::string_view value) {
    std::size_t begin = 0U;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && (value[end - 1U] == ' ' || value[end - 1U] == '\t')) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

[[nodiscard]] std::string to_lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const unsigned char character : text) {
        if (character >= 'A' && character <= 'Z') {
            result.push_back(static_cast<char>(character - 'A' + 'a'));
        } else {
            result.push_back(static_cast<char>(character));
        }
    }
    return result;
}

[[nodiscard]] bool valid_field_value(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return character == '\t' || character == ' ' ||
               (character >= 0x21U && character <= 0x7eU) || character >= 0x80U;
    });
}

[[nodiscard]] bool parse_request_line_fields(std::string_view line, HttpRequest& request) {
    const std::size_t first_space = line.find(' ');
    if (first_space == std::string_view::npos || first_space == 0U) {
        return false;
    }
    const std::size_t second_space = line.find(' ', first_space + 1U);
    if (second_space == std::string_view::npos || second_space == first_space + 1U ||
        second_space + 1U >= line.size()) {
        return false;
    }
    if (line.find(' ', second_space + 1U) != std::string_view::npos ||
        line.find('\t') != std::string_view::npos) {
        return false;
    }

    const std::string_view method = line.substr(0U, first_space);
    const std::string_view target =
        line.substr(first_space + 1U, second_space - first_space - 1U);
    const std::string_view version = line.substr(second_space + 1U);

    if (!valid_method(method) || !valid_target(target)) {
        return false;
    }
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        return false;
    }

    request.method = std::string(method);
    request.target = std::string(target);
    request.version = std::string(version);
    return true;
}

[[nodiscard]] bool parse_header_line(std::string_view line, HttpRequest& request) {
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0U) {
        return false;
    }

    const std::string_view raw_name = line.substr(0U, colon);
    if (!std::all_of(raw_name.begin(), raw_name.end(), [](unsigned char character) {
            return is_token_character(character);
        })) {
        return false;
    }

    const std::string_view raw_value = line.substr(colon + 1U);
    if (!valid_field_value(raw_value)) {
        return false;
    }

    std::string name = to_lower(raw_name);
    std::string value = trim_ows(raw_value);
    if (request.headers.find(name) != request.headers.end()) {
        return false;
    }

    request.headers.emplace(std::move(name), std::move(value));
    return true;
}

[[nodiscard]] bool parse_content_length(std::string_view text, std::size_t& result) {
    if (text.empty()) {
        return false;
    }

    std::size_t value = 0U;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end) {
        return false;
    }

    result = value;
    return true;
}

[[nodiscard]] ParseResult make_error(std::string message, HttpRequest request = {}) {
    ParseResult result;
    result.status = ParseStatus::Error;
    result.request = std::move(request);
    result.error = std::move(message);
    return result;
}

}  // namespace

ParseResult HttpRequestParser::parse(std::string_view buffer) const {
    ParseResult result;

    const std::size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        if (buffer.size() > kMaxHeaderSize) {
            return make_error("HTTP header too large");
        }
        return result;
    }

    const std::size_t header_bytes = header_end + 4U;
    if (header_bytes > kMaxHeaderSize) {
        return make_error("HTTP header too large");
    }

    const std::size_t request_line_end = buffer.find("\r\n");
    if (request_line_end == std::string_view::npos || request_line_end > header_end) {
        return make_error("invalid request line");
    }

    if (!parse_request_line_fields(buffer.substr(0U, request_line_end), result.request)) {
        return make_error("invalid request line", std::move(result.request));
    }

    std::size_t position = request_line_end + 2U;
    while (position < header_end) {
        const std::size_t line_end = buffer.find("\r\n", position);
        if (line_end == std::string_view::npos || line_end > header_end) {
            return make_error("invalid header", std::move(result.request));
        }

        const std::string_view line = buffer.substr(position, line_end - position);
        if (line.empty() || !parse_header_line(line, result.request)) {
            return make_error("invalid or duplicate header", std::move(result.request));
        }
        position = line_end + 2U;
    }

    if (result.request.version == "HTTP/1.1" &&
        result.request.headers.find("host") == result.request.headers.end()) {
        return make_error("missing Host header", std::move(result.request));
    }

    if (result.request.headers.find("transfer-encoding") != result.request.headers.end()) {
        return make_error("Transfer-Encoding is not supported", std::move(result.request));
    }

    std::size_t content_length = 0U;
    const auto content_length_header = result.request.headers.find("content-length");
    if (content_length_header != result.request.headers.end()) {
        if (!parse_content_length(content_length_header->second, content_length)) {
            return make_error("invalid Content-Length", std::move(result.request));
        }
        if (content_length > kMaxBodySize) {
            return make_error("HTTP body too large", std::move(result.request));
        }
    }

    if (content_length > std::numeric_limits<std::size_t>::max() - header_bytes) {
        return make_error("invalid message size", std::move(result.request));
    }
    const std::size_t total_bytes = header_bytes + content_length;

    if (buffer.size() < total_bytes) {
        result.status = ParseStatus::NeedMoreData;
        return result;
    }

    result.request.body = std::string(buffer.substr(header_bytes, content_length));
    result.consumed = total_bytes;
    result.status = ParseStatus::Complete;
    return result;
}

}  // namespace cppapplab::http
