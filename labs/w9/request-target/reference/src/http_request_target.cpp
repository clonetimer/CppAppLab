#include "http_request_target.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace cppapplab::http {
namespace {

constexpr std::size_t kMaxTargetSize = 8192U;
constexpr std::size_t kMaxPathSize = 4096U;
constexpr std::size_t kMaxQuerySize = 4096U;
constexpr std::size_t kMaxComponentSize = 1024U;
constexpr std::size_t kMaxQueryParameters = 128U;
constexpr std::string_view kTokenPunctuation = "!#$%&'*+-.^_|~";

[[nodiscard]] bool is_token_character(unsigned char character) noexcept {
    const bool alpha_numeric = (character >= 'a' && character <= 'z') ||
                               (character >= 'A' && character <= 'Z') ||
                               (character >= '0' && character <= '9');
    return alpha_numeric || character == 0x60U ||
           kTokenPunctuation.find(static_cast<char>(character)) != std::string_view::npos;
}

[[nodiscard]] bool valid_method(std::string_view method) noexcept {
    if (method.empty()) {
        return false;
    }
    return std::all_of(method.begin(), method.end(), [](unsigned char character) {
        return is_token_character(character);
    });
}

[[nodiscard]] int hex_value(unsigned char character) noexcept {
    if (character >= '0' && character <= '9') {
        return static_cast<int>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<int>(character - 'a' + 10U);
    }
    if (character >= 'A' && character <= 'F') {
        return static_cast<int>(character - 'A' + 10U);
    }
    return -1;
}

struct DecodeResult {
    bool ok{false};
    std::string value;
    std::string error;
};

[[nodiscard]] DecodeResult decode_component(std::string_view raw, bool path_component,
                                            std::size_t max_size) {
    DecodeResult result;
    if (raw.size() > max_size) {
        result.error = "request-target component too large";
        return result;
    }

    result.value.reserve(raw.size());
    for (std::size_t index = 0U; index < raw.size(); ++index) {
        unsigned char decoded = static_cast<unsigned char>(raw[index]);
        bool percent_encoded = false;
        if (decoded == '%') {
            percent_encoded = true;
            if (index + 2U >= raw.size()) {
                result.error = "invalid percent-encoding";
                return result;
            }
            const int high = hex_value(static_cast<unsigned char>(raw[index + 1U]));
            const int low = hex_value(static_cast<unsigned char>(raw[index + 2U]));
            if (high < 0 || low < 0) {
                result.error = "invalid percent-encoding";
                return result;
            }
            decoded = static_cast<unsigned char>((high << 4) | low);
            index += 2U;
        }

        if (decoded == 0U || decoded < 0x20U || decoded == 0x7fU) {
            result.error = "decoded request target contains a control byte";
            return result;
        }
        if (path_component && percent_encoded && (decoded == '/' || decoded == '\\')) {
            result.error = "encoded path separator is not allowed";
            return result;
        }

        result.value.push_back(static_cast<char>(decoded));
        if (result.value.size() > max_size) {
            result.error = "request-target component too large";
            return result;
        }
    }

    result.ok = true;
    return result;
}

[[nodiscard]] RequestTargetParseResult target_error(std::string message) {
    RequestTargetParseResult result;
    result.status = RequestTargetStatus::Error;
    result.error = std::move(message);
    return result;
}

[[nodiscard]] bool contains_dot_segment(std::string_view path) noexcept {
    std::size_t segment_start = 1U;
    while (segment_start <= path.size()) {
        const std::size_t separator = path.find('/', segment_start);
        const std::size_t segment_end =
            separator == std::string_view::npos ? path.size() : separator;
        const std::string_view segment = path.substr(segment_start, segment_end - segment_start);
        if (segment == "." || segment == "..") {
            return true;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segment_start = separator + 1U;
    }
    return false;
}

[[nodiscard]] bool valid_raw_target(std::string_view raw_target) noexcept {
    for (const unsigned char character : raw_target) {
        if (character <= 0x20U || character >= 0x7fU || character == '#' || character == '\\') {
            return false;
        }
    }
    return true;
}

}  // namespace

RequestTargetParseResult parse_request_target(std::string_view method,
                                              std::string_view raw_target) {
    if (!valid_method(method)) {
        return target_error("invalid HTTP method");
    }
    if (raw_target.empty()) {
        return target_error("empty request target");
    }
    if (raw_target.size() > kMaxTargetSize) {
        return target_error("request target too large");
    }
    if (!valid_raw_target(raw_target)) {
        return target_error("request target contains an invalid raw byte or fragment");
    }

    if (raw_target == "*") {
        if (method != "OPTIONS") {
            return target_error("asterisk-form is only valid for OPTIONS");
        }
        RequestTargetParseResult result;
        result.status = RequestTargetStatus::Complete;
        result.target.asterisk_form = true;
        return result;
    }

    if (raw_target.front() != '/') {
        return target_error("unsupported request-target form");
    }

    const std::size_t query_separator = raw_target.find('?');
    const std::string_view raw_path =
        query_separator == std::string_view::npos ? raw_target : raw_target.substr(0U, query_separator);
    const std::string_view raw_query =
        query_separator == std::string_view::npos ? std::string_view{} :
                                                    raw_target.substr(query_separator + 1U);

    if (raw_path.empty() || raw_path.front() != '/') {
        return target_error("invalid origin-form path");
    }
    if (raw_path.size() > kMaxPathSize) {
        return target_error("request path too large");
    }
    if (raw_query.size() > kMaxQuerySize) {
        return target_error("query string too large");
    }

    DecodeResult decoded_path = decode_component(raw_path, true, kMaxPathSize);
    if (!decoded_path.ok) {
        return target_error(std::move(decoded_path.error));
    }
    if (contains_dot_segment(decoded_path.value)) {
        return target_error("request path contains dot segment");
    }

    RequestTargetParseResult result;
    result.status = RequestTargetStatus::Complete;
    result.target.path = std::move(decoded_path.value);

    if (raw_query.empty()) {
        return result;
    }

    std::size_t parameter_start = 0U;
    while (parameter_start <= raw_query.size()) {
        if (result.target.query.size() >= kMaxQueryParameters) {
            return target_error("query parameter limit exceeded");
        }

        const std::size_t separator = raw_query.find('&', parameter_start);
        const std::size_t parameter_end =
            separator == std::string_view::npos ? raw_query.size() : separator;
        const std::string_view parameter =
            raw_query.substr(parameter_start, parameter_end - parameter_start);
        if (parameter.empty()) {
            return target_error("empty query parameter");
        }

        const std::size_t equals = parameter.find('=');
        const std::string_view raw_name =
            equals == std::string_view::npos ? parameter : parameter.substr(0U, equals);
        const std::string_view raw_value =
            equals == std::string_view::npos ? std::string_view{} : parameter.substr(equals + 1U);
        if (raw_name.empty()) {
            return target_error("query parameter name is empty");
        }

        DecodeResult name = decode_component(raw_name, false, kMaxComponentSize);
        if (!name.ok) {
            return target_error(std::move(name.error));
        }
        DecodeResult value = decode_component(raw_value, false, kMaxComponentSize);
        if (!value.ok) {
            return target_error(std::move(value.error));
        }

        result.target.query.push_back(
            QueryParameter{std::move(name.value), std::move(value.value)});

        if (separator == std::string_view::npos) {
            break;
        }
        parameter_start = separator + 1U;
    }

    return result;
}

}  // namespace cppapplab::http
