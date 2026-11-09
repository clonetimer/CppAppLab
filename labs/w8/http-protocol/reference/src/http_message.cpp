#include "http_message.hpp"

#include <algorithm>
#include <cctype>
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

std::string serialize_response(const HttpResponse& response) {
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
        "HTTP/1.1 " + std::to_string(response.status_code) + " " + response.reason_phrase + "\r\n";
    for (const auto& entry : headers) {
        output += entry.first + ": " + entry.second + "\r\n";
    }
    output += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    output += "Connection: close\r\n";
    output += "\r\n";
    output += response.body;
    return output;
}

}  // namespace cppapplab::http
