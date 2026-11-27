#include "http_hardening.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <utility>

namespace cppapplab::http {
namespace {

[[nodiscard]] AdmissionResult reject(AdmissionStatus status) {
    AdmissionResult result;
    result.status = status;
    result.close_connection = true;
    return result;
}

[[nodiscard]] AdmissionStatus header_budget(std::string_view buffer) {
    const auto prefix = buffer.substr(0U, std::min(buffer.size(), kAdmissionHeaderBytes));
    const auto end = prefix.find("\r\n\r\n");
    if (end == std::string_view::npos) {
        return buffer.size() >= kAdmissionHeaderBytes ? AdmissionStatus::HeaderTooLarge
                                                     : AdmissionStatus::NeedMoreData;
    }
    // Count field lines only, not the request line or terminating blank line.
    const auto first = prefix.find("\r\n");
    if (first == std::string_view::npos || first > end) {
        return AdmissionStatus::BadRequest;
    }
    std::size_t cursor = first + 2U;
    std::size_t fields = 0U;
    while (cursor < end) {
        const auto next = prefix.find("\r\n", cursor);
        if (next == std::string_view::npos || next > end) {
            return AdmissionStatus::BadRequest;
        }
        if (++fields > kAdmissionHeaderFields) {
            return AdmissionStatus::HeaderTooLarge;
        }
        cursor = next + 2U;
    }
    return AdmissionStatus::Accepted;
}

[[nodiscard]] AdmissionStatus parser_error(std::string_view error) {
    if (error == "HTTP header too large" || error == "HTTP trailer section too large") {
        return AdmissionStatus::HeaderTooLarge;
    }
    if (error == "HTTP body too large") {
        return AdmissionStatus::PayloadTooLarge;
    }
    return AdmissionStatus::BadRequest;
}

}  // namespace

AdmissionResult admit_request(std::string_view buffer) {
    const auto header = header_budget(buffer);
    if (header == AdmissionStatus::NeedMoreData) {
        return {};
    }
    if (header != AdmissionStatus::Accepted) {
        return reject(header);
    }

    // Only the first bounded wire window may be passed to the fixed A1 parser.
    // A completed first request must not be rejected merely because a suffix exists.
    const auto bounded = buffer.substr(0U, std::min(buffer.size(), kAdmissionWireBytes));
    auto parsed = HttpRequestParser{}.parse(bounded);
    if (parsed.status == ParseStatus::Error) {
        return reject(parser_error(parsed.error));
    }

    const auto host = parsed.request.headers.find("host");
    if (parsed.request.version == "HTTP/1.1" &&
        (host == parsed.request.headers.end() || host->second.empty())) {
        return reject(AdmissionStatus::BadRequest);
    }
    const auto length = parsed.request.headers.find("content-length");
    if (length != parsed.request.headers.end()) {
        std::size_t bytes = 0U;
        const auto& value = length->second;
        const auto* end = value.data() + value.size();
        const auto conversion = std::from_chars(value.data(), end, bytes);
        if (conversion.ec != std::errc{} || conversion.ptr != end) {
            return reject(AdmissionStatus::BadRequest);
        }
        if (bytes > kAdmissionBodyBytes) {
            return reject(AdmissionStatus::PayloadTooLarge);
        }
    }

    if (parsed.status == ParseStatus::NeedMoreData) {
        if (buffer.size() >= kAdmissionWireBytes) {
            return reject(AdmissionStatus::PayloadTooLarge);
        }
        return {};
    }
    if (parsed.consumed == 0U || parsed.consumed > bounded.size()) {
        return reject(AdmissionStatus::BadRequest);
    }
    if (parsed.request.body.size() > kAdmissionBodyBytes) {
        return reject(AdmissionStatus::PayloadTooLarge);
    }
    AdmissionResult result;
    result.status = AdmissionStatus::Accepted;
    result.consumed = parsed.consumed;
    result.request = std::move(parsed.request);
    return result;
}

}  // namespace cppapplab::http
