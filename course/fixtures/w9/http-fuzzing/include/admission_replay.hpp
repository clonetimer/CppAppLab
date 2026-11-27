#pragma once

#include "admission_invariants.hpp"

namespace cppapplab::http {

// Platform-owned driver: actual A4 execution plus the learner oracle.
// Reparse the same bytes and, for an accepted request, the exact consumed prefix.
// This exercises repeatability and suffix independence, not protocol completeness.
inline InvariantViolation verify_admission_input(std::string_view input) {
    const auto first = admit_request(input);
    const auto violation = validate_observation(input, first);
    if (violation != InvariantViolation::None) {
        return violation;
    }
    const auto repeated = admit_request(input);
    if (!same_observation(first, repeated)) {
        return InvariantViolation::ReplayMismatch;
    }
    if (first.status == AdmissionStatus::Accepted) {
        // Guard slicing independently even if a learner oracle is incomplete.
        if (first.consumed == 0U || first.consumed > input.size()) {
            return InvariantViolation::ConsumedBoundary;
        }
        const auto prefix = input.substr(0U, first.consumed);
        const auto exact = admit_request(prefix);
        if (validate_observation(prefix, exact) != InvariantViolation::None ||
            !same_observation(first, exact)) {
            return InvariantViolation::PrefixMismatch;
        }
        const auto shorter = prefix.substr(0U, prefix.size() - 1U);
        const auto partial = admit_request(shorter);
        if (partial.status != AdmissionStatus::NeedMoreData ||
            validate_observation(shorter, partial) != InvariantViolation::None) {
            return InvariantViolation::PrefixMismatch;
        }
    }
    return InvariantViolation::None;
}

}  // namespace cppapplab::http
