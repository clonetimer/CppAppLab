#include "admission_invariants.hpp"

namespace cppapplab::http {
namespace {

[[nodiscard]] bool empty_request(const HttpRequest& request) {
    return request.method.empty() && request.target.empty() && request.version.empty() &&
           request.headers.empty() && request.body.empty();
}

}  // namespace

InvariantViolation validate_observation(std::string_view input,
                                        const AdmissionResult& result) {
    switch (result.status) {
        case AdmissionStatus::NeedMoreData:
        case AdmissionStatus::BadRequest:
        case AdmissionStatus::HeaderTooLarge:
        case AdmissionStatus::PayloadTooLarge:
            if (result.consumed != 0U || !empty_request(result.request)) {
                return InvariantViolation::HiddenState;
            }
            if (result.close_connection != (result.status != AdmissionStatus::NeedMoreData)) {
                return InvariantViolation::ClosePolicy;
            }
            return InvariantViolation::None;
        case AdmissionStatus::Accepted:
            break;
        default:
            return InvariantViolation::UnknownStatus;
    }
    if (result.close_connection) {
        return InvariantViolation::ClosePolicy;
    }
    if (result.consumed == 0U || result.consumed > input.size() ||
        result.consumed > kAdmissionWireBytes) {
        return InvariantViolation::ConsumedBoundary;
    }
    const auto& request = result.request;
    if (request.method.empty() || request.target.empty() ||
        (request.version != "HTTP/1.0" && request.version != "HTTP/1.1")) {
        return InvariantViolation::RequestShape;
    }
    const auto host = request.headers.find("host");
    if (request.version == "HTTP/1.1" &&
        (host == request.headers.end() || host->second.empty())) {
        return InvariantViolation::RequestShape;
    }
    const auto first_message = input.substr(0U, result.consumed);
    const auto header_end = first_message.find("\r\n\r\n");
    if (header_end == std::string_view::npos ||
        header_end + 4U > kAdmissionHeaderBytes) {
        return InvariantViolation::HeaderBoundary;
    }
    if (request.headers.size() > kAdmissionHeaderFields ||
        request.body.size() > kAdmissionBodyBytes ||
        request.body.size() > first_message.size() - (header_end + 4U)) {
        return InvariantViolation::ResourceBudget;
    }
    return InvariantViolation::None;
}

bool same_observation(const AdmissionResult& left, const AdmissionResult& right) {
    return left.status == right.status && left.consumed == right.consumed &&
           left.close_connection == right.close_connection &&
           left.request.method == right.request.method &&
           left.request.target == right.request.target &&
           left.request.version == right.request.version &&
           left.request.headers == right.request.headers &&
           left.request.body == right.request.body;
}

}  // namespace cppapplab::http
