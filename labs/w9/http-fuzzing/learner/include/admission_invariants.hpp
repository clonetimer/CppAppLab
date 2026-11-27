#pragma once

#include "http_hardening.hpp"

#include <string_view>

namespace cppapplab::http {

// A learner-owned oracle checks observable admission invariants. It does not
// implement another HTTP grammar or claim that all accepted messages are safe.
enum class InvariantViolation {
    None,
    UnknownStatus,
    HiddenState,
    ClosePolicy,
    ConsumedBoundary,
    RequestShape,
    HeaderBoundary,
    ResourceBudget,
    ReplayMismatch,
    PrefixMismatch,
};

[[nodiscard]] InvariantViolation validate_observation(
    std::string_view input, const AdmissionResult& result);
[[nodiscard]] bool same_observation(const AdmissionResult& left,
                                    const AdmissionResult& right);

}  // namespace cppapplab::http
