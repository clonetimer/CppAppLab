#include "http_request_parser.hpp"

#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace cppapplab::http {
namespace {

constexpr std::size_t kMaxHeaderSize = 16U * 1024U;
constexpr std::size_t kMaxBodySize = 1024U * 1024U;

[[nodiscard]] std::string trim(std::string_view value) {
    std::size_t begin = 0U;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0) {
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

[[nodiscard]] bool parse_request_line(std::string_view line, HttpRequest& request) {
    std::istringstream input{std::string(line)};
    std::string method;
    std::string target;
    std::string version;
    std::string extra;
    if (!(input >> method >> target >> version) || (input >> extra)) {
        return false;
    }
    if (method.empty() || target.empty()) {
        return false;
    }
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        return false;
    }

    request.method = std::move(method);
    request.target = std::move(target);
    request.version = std::move(version);
    return true;
}

[[nodiscard]] bool parse_header_line(std::string_view line, HttpRequest& request) {
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0U) {
        return false;
    }

    std::string name = to_lower(trim(line.substr(0U, colon)));
    std::string value = trim(line.substr(colon + 1U));
    if (name.empty() || request.headers.find(name) != request.headers.end()) {
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

    if (!parse_request_line(buffer.substr(0U, request_line_end), result.request)) {
        return make_error("invalid request line", std::move(result.request));
    }

    std::size_t position = request_line_end + 2U;
    while (position < header_end) {
        const std::size_t line_end = buffer.find("\r\n", position);
        if (line_end == std::string_view::npos || line_end > header_end) {
            return make_error("invalid header", std::move(result.request));
        }

        const std::string_view line = buffer.substr(position, line_end - position);
        if (line.empty()) {
            return make_error("unexpected empty header", std::move(result.request));
        }
        if (!parse_header_line(line, result.request)) {
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
