#pragma once

#include "http_request_parser.hpp"

#include <cstddef>
#include <string_view>

namespace cppapplab::http {

// Deliberately stricter local service policy than the reusable A1 parser.
// These constants bound a single request, not the complete pipeline buffer.
inline constexpr std::size_t kAdmissionHeaderBytes = 8192U;
inline constexpr std::size_t kAdmissionHeaderFields = 32U;
inline constexpr std::size_t kAdmissionBodyBytes = 65536U;
inline constexpr std::size_t kAdmissionWireBytes = 98304U;

enum class AdmissionStatus {
    NeedMoreData,
    Accepted,
    BadRequest,
    HeaderTooLarge,
    PayloadTooLarge,
};

struct AdmissionResult {
    AdmissionStatus status{AdmissionStatus::NeedMoreData};
    HttpRequest request;
    std::size_t consumed{0U};
    // Mandatory policy close, not the complete HTTP connection negotiation.
    bool close_connection{false};
};

[[nodiscard]] AdmissionResult admit_request(std::string_view buffer);

}  // namespace cppapplab::http
