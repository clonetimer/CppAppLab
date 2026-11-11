#include "http_request_target.hpp"

namespace cppapplab::http {

RequestTargetParseResult parse_request_target(std::string_view method,
                                              std::string_view raw_target) {
    (void)method;
    (void)raw_target;

    // TODO(student): validate request-target form, decode path/query once and enforce limits.
    RequestTargetParseResult result;
    result.status = RequestTargetStatus::Error;
    result.error = "TODO(student): request-target parser is not implemented";
    return result;
}

}  // namespace cppapplab::http
