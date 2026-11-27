#include "admission_invariants.hpp"

namespace cppapplab::http {

InvariantViolation validate_observation(std::string_view input,
                                        const AdmissionResult& result) {
    // TODO(W9-A5): verify the returned state, resource limits and consumed boundary.
    (void)input;
    (void)result;
    return InvariantViolation::None;
}

bool same_observation(const AdmissionResult& left, const AdmissionResult& right) {
    // TODO(W9-A5): compare every observable field; header order is not semantic.
    (void)left;
    (void)right;
    return true;
}

}  // namespace cppapplab::http
