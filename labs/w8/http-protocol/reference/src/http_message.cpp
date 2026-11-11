#include "http_message.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cppapplab::http {
namespace {

constexpr std::string_view kHeaderPunctuation = "!#$%&'*+-.^_`|~";

[[nodiscard]] bool has_forbidden_line_break(std::string_view value) {
    return value.find('\r') != std::string_view::npos || value.find('\n') != std::string_view::npos;
}

[[nodiscard]] bool valid_header_name(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char character) {
        const bool alpha_numeric = (character >= 'a' && character <= 'z') ||
                                   (character >= 'A' && character <= 'Z') ||
                                   (character >= '0' && character <= '9');
        return alpha_numeric ||
               kHeaderPunctuation.find(static_cast<char>(character)) != std::string_view::npos;
    });
}

[[nodiscard]] std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character - 'A' + 'a');
        }
        return static_cast<char>(character);
    });
    return value;
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

[[nodiscard]] bool contains_connection_option(std::string_view value,
                                              std::string_view expected) noexcept {
    std::size_t begin = 0U;
    while (begin <= value.size()) {
        const std::size_t comma = value.find(',', begin);
        const std::size_t end = comma == std::string_view::npos ? value.size() : comma;
        std::size_t token_begin = begin;
        std::size_t token_end = end;

        while (token_begin < token_end &&
               (value[token_begin] == ' ' || value[token_begin] == '\t')) {
            ++token_begin;
        }
        while (token_end > token_begin &&
               (value[token_end - 1U] == ' ' || value[token_end - 1U] == '\t')) {
            --token_end;
        }

        if (ascii_iequals(value.substr(token_begin, token_end - token_begin), expected)) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1U;
    }
    return false;
}

void validate_header_value(std::string_view value) {
    for (const unsigned char character : value) {
        const bool line_break = character == '\r' || character == '\n';
        const bool control = character < 0x20U && character != '\t';
        if (line_break || control || character == 0x7fU) {
            throw std::invalid_argument("HTTP header value contains a forbidden control byte");
        }
    }
}

}  // namespace

bool parse_request_line(std::string_view line, HttpRequest& request) {
    if (line.empty() || has_forbidden_line_break(line)) {
        return false;
    }

    std::istringstream input{std::string(line)};
    std::string method;
    std::string target;
    std::string version;
    std::string extra;
    if (!(input >> method >> target >> version) || (input >> extra)) {
        return false;
    }

    HttpRequest parsed = request;
    parsed.method = std::move(method);
    parsed.target = std::move(target);
    parsed.version = std::move(version);
    request = std::move(parsed);
    return true;
}

ConnectionPolicy request_connection_policy(const HttpRequest& request) noexcept {
    bool has_close = false;
    bool has_keep_alive = false;

    for (const auto& header : request.headers) {
        if (!ascii_iequals(header.first, "connection")) {
            continue;
        }
        has_close = has_close || contains_connection_option(header.second, "close");
        has_keep_alive = has_keep_alive || contains_connection_option(header.second, "keep-alive");
    }

    // "close" always wins, including when both options are present.
    if (has_close) {
        return ConnectionPolicy::Close;
    }
    if (request.version == "HTTP/1.1") {
        return ConnectionPolicy::KeepAlive;
    }
    if (request.version == "HTTP/1.0" && has_keep_alive) {
        return ConnectionPolicy::KeepAlive;
    }
    return ConnectionPolicy::Close;
}

std::string serialize_response(const HttpResponse& response) {
    return serialize_response(response, ConnectionPolicy::Close, "HTTP/1.1");
}

std::string serialize_response(const HttpResponse& response, ConnectionPolicy connection_policy) {
    return serialize_response(response, connection_policy, "HTTP/1.1");
}

std::string serialize_response(const HttpResponse& response, ConnectionPolicy connection_policy,
                               std::string_view response_version) {
    if (response_version != "HTTP/1.0" && response_version != "HTTP/1.1") {
        throw std::invalid_argument("HTTP response version must be HTTP/1.0 or HTTP/1.1");
    }
    if (response.status_code < 100 || response.status_code > 599) {
        throw std::invalid_argument("HTTP status code must be in the range 100..599");
    }
    if (response.reason_phrase.empty() || has_forbidden_line_break(response.reason_phrase)) {
        throw std::invalid_argument("HTTP reason phrase must be non-empty and single-line");
    }

    std::vector<std::pair<std::string, std::string>> headers;
    headers.reserve(response.headers.size());
    for (const auto& entry : response.headers) {
        if (!valid_header_name(entry.first)) {
            throw std::invalid_argument("HTTP header name is invalid");
        }
        validate_header_value(entry.second);
        const std::string name = lowercase(entry.first);
        if (name == "content-length" || name == "connection") {
            continue;
        }
        headers.emplace_back(entry.first, entry.second);
    }

    std::sort(headers.begin(), headers.end(), [](const auto& left, const auto& right) {
        return lowercase(left.first) < lowercase(right.first);
    });
    for (std::size_t index = 1U; index < headers.size(); ++index) {
        if (lowercase(headers[index - 1U].first) == lowercase(headers[index].first)) {
            throw std::invalid_argument(
                "HTTP header names are case-insensitive and must be unique");
        }
    }

    std::string output =
        std::string(response_version) + " " + std::to_string(response.status_code) + " " +
        response.reason_phrase + "\r\n";
    for (const auto& entry : headers) {
        output += entry.first + ": " + entry.second + "\r\n";
    }
    output += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    output += connection_policy == ConnectionPolicy::KeepAlive ? "Connection: keep-alive\r\n"
                                                               : "Connection: close\r\n";
    output += "\r\n";
    output += response.body;
    return output;
}

}  // namespace cppapplab::http
