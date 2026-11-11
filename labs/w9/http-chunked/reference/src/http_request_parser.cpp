#include "http_request_parser.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    if (method.empty()) {
        return false;
    }
    for (const unsigned char character : method) {
        if (!is_token_character(character)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_target(std::string_view target) {
    if (target.empty()) {
        return false;
    }
    for (const unsigned char character : target) {
        if (character <= 0x20U || character >= 0x7fU) {
            return false;
        }
    }
    return true;
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
    const bool extra_space = line.find(' ', second_space + 1U) != std::string_view::npos;
    const bool tab_separator = line.find('\t') != std::string_view::npos;
    if (extra_space || tab_separator) {
        return false;
    }

    const std::string_view method = line.substr(0U, first_space);
    const std::size_t target_start = first_space + 1U;
    const std::size_t target_length = second_space - target_start;
    const std::string_view target = line.substr(target_start, target_length);
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

[[nodiscard]] bool valid_header_name(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    for (const unsigned char character : name) {
        if (!is_token_character(character)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_header_line(std::string_view line, HttpRequest& request) {
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0U) {
        return false;
    }

    const std::string_view raw_name = line.substr(0U, colon);
    if (!valid_header_name(raw_name)) {
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

[[nodiscard]] bool ascii_iequals(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < left.size(); ++index) {
        unsigned char lhs = static_cast<unsigned char>(left[index]);
        unsigned char rhs = static_cast<unsigned char>(right[index]);
        if (lhs >= 'A' && lhs <= 'Z') {
            lhs = static_cast<unsigned char>(lhs - 'A' + 'a');
        }
        if (rhs >= 'A' && rhs <= 'Z') {
            rhs = static_cast<unsigned char>(rhs - 'A' + 'a');
        }
        if (lhs != rhs) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_supported_transfer_encoding(std::string_view value) noexcept {
    // This task intentionally supports exactly one transfer coding: chunked.
    // A coding chain such as "gzip, chunked" requires decoding an additional layer.
    return ascii_iequals(trim_ows(value), "chunked");
}

[[nodiscard]] bool valid_chunk_extension(std::string_view extension) noexcept {
    if (extension.empty()) {
        return false;
    }
    return std::all_of(extension.begin(), extension.end(), [](unsigned char character) {
        return character >= 0x20U && character <= 0x7eU;
    });
}

enum class ChunkedParseStatus { NeedMoreData, Complete, Error };

struct ChunkedParseResult {
    ChunkedParseStatus status{ChunkedParseStatus::NeedMoreData};
    std::string body;
    std::size_t consumed{0U};
    std::string error;
};

[[nodiscard]] bool forbidden_trailer_name(std::string_view name) noexcept {
    return name == "content-length" || name == "transfer-encoding" || name == "host" ||
           name == "connection" || name == "trailer" || name == "upgrade";
}

[[nodiscard]] ChunkedParseResult chunked_error(std::string message) {
    ChunkedParseResult result;
    result.status = ChunkedParseStatus::Error;
    result.error = std::move(message);
    return result;
}

[[nodiscard]] ChunkedParseResult parse_chunked_body(std::string_view buffer,
                                                    std::size_t body_start) {
    constexpr std::size_t kMaxChunkLineSize = 1024U;
    std::size_t cursor = body_start;
    std::string body;

    for (;;) {
        const std::size_t line_end = buffer.find("\r\n", cursor);
        if (line_end == std::string_view::npos) {
            if (buffer.size() - cursor > kMaxChunkLineSize) {
                return chunked_error("HTTP chunk-size line too large");
            }
            return {};
        }
        if (line_end - cursor > kMaxChunkLineSize) {
            return chunked_error("HTTP chunk-size line too large");
        }

        const std::string_view line = buffer.substr(cursor, line_end - cursor);
        const std::size_t extension_separator = line.find(';');
        const std::string_view size_text = line.substr(0U, extension_separator);
        if (size_text.empty()) {
            return chunked_error("invalid HTTP chunk size");
        }

        std::size_t chunk_size = 0U;
        const char* const size_begin = size_text.data();
        const char* const size_end = size_text.data() + size_text.size();
        const auto [size_position, size_error] =
            std::from_chars(size_begin, size_end, chunk_size, 16);
        if (size_error != std::errc{} || size_position != size_end) {
            return chunked_error("invalid HTTP chunk size");
        }

        if (extension_separator != std::string_view::npos &&
            !valid_chunk_extension(line.substr(extension_separator + 1U))) {
            return chunked_error("invalid HTTP chunk extension");
        }

        cursor = line_end + 2U;
        if (chunk_size == 0U) {
            const std::size_t trailer_start = cursor;
            std::vector<std::string> trailer_names;

            for (;;) {
                const std::size_t trailer_end = buffer.find("\r\n", cursor);
                if (trailer_end == std::string_view::npos) {
                    if (buffer.size() - trailer_start > kMaxHeaderSize) {
                        return chunked_error("HTTP trailer section too large");
                    }
                    return {};
                }

                const std::size_t trailer_bytes = trailer_end + 2U - trailer_start;
                if (trailer_bytes > kMaxHeaderSize) {
                    return chunked_error("HTTP trailer section too large");
                }

                const std::string_view trailer_line = buffer.substr(cursor, trailer_end - cursor);
                if (trailer_line.empty()) {
                    ChunkedParseResult result;
                    result.status = ChunkedParseStatus::Complete;
                    result.body = std::move(body);
                    result.consumed = trailer_end + 2U;
                    return result;
                }

                const std::size_t colon = trailer_line.find(':');
                if (colon == std::string_view::npos || colon == 0U) {
                    return chunked_error("invalid HTTP trailer field");
                }

                const std::string_view raw_name = trailer_line.substr(0U, colon);
                const std::string_view raw_value = trailer_line.substr(colon + 1U);
                if (!valid_header_name(raw_name) || !valid_field_value(raw_value)) {
                    return chunked_error("invalid HTTP trailer field");
                }

                const std::string name = to_lower(raw_name);
                if (forbidden_trailer_name(name)) {
                    return chunked_error("forbidden HTTP trailer field");
                }
                if (std::find(trailer_names.begin(), trailer_names.end(), name) !=
                    trailer_names.end()) {
                    return chunked_error("duplicate HTTP trailer field");
                }
                trailer_names.push_back(name);
                cursor = trailer_end + 2U;
            }
        }

        if (chunk_size > kMaxBodySize - body.size()) {
            return chunked_error("HTTP body too large");
        }
        if (chunk_size > std::numeric_limits<std::size_t>::max() - 2U) {
            return chunked_error("invalid HTTP chunk size");
        }
        const std::size_t chunk_wire_bytes = chunk_size + 2U;
        if (buffer.size() - cursor < chunk_wire_bytes) {
            return {};
        }

        if (buffer[cursor + chunk_size] != '\r' || buffer[cursor + chunk_size + 1U] != '\n') {
            return chunked_error("HTTP chunk data is not followed by CRLF");
        }

        body.append(buffer.substr(cursor, chunk_size));
        cursor += chunk_wire_bytes;
    }
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

    const auto transfer_encoding_header = result.request.headers.find("transfer-encoding");
    const auto content_length_header = result.request.headers.find("content-length");

    if (transfer_encoding_header != result.request.headers.end()) {
        if (result.request.version != "HTTP/1.1") {
            return make_error("Transfer-Encoding requires HTTP/1.1", std::move(result.request));
        }
        if (content_length_header != result.request.headers.end()) {
            return make_error("Content-Length and Transfer-Encoding cannot be combined",
                              std::move(result.request));
        }
        if (!is_supported_transfer_encoding(transfer_encoding_header->second)) {
            return make_error("unsupported Transfer-Encoding", std::move(result.request));
        }

        ChunkedParseResult chunked = parse_chunked_body(buffer, header_bytes);
        if (chunked.status == ChunkedParseStatus::NeedMoreData) {
            return result;
        }
        if (chunked.status == ChunkedParseStatus::Error) {
            return make_error(std::move(chunked.error), std::move(result.request));
        }

        result.request.body = std::move(chunked.body);
        result.consumed = chunked.consumed;
        result.status = ParseStatus::Complete;
        return result;
    }

    std::size_t content_length = 0U;
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
