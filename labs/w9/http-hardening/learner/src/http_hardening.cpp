#include "http_hardening.hpp"

namespace cppapplab::http {

AdmissionResult admit_request(std::string_view buffer) {
    // TODO(W9-A4): add the admission policy around the supplied A1 parser.
    // No partial request may escape on NeedMoreData or an error.
    (void)buffer;
    return {};
}

}  // namespace cppapplab::http
