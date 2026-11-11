#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cppapplab::http {

struct QueryParameter {
    std::string name;
    std::string value;
};

struct ParsedRequestTarget {
    std::string path;
    std::vector<QueryParameter> query;
    bool asterisk_form{false};
};

enum class RequestTargetStatus { Complete, Error };

struct RequestTargetParseResult {
    RequestTargetStatus status{RequestTargetStatus::Error};
    ParsedRequestTarget target;
    std::string error;
};

[[nodiscard]] RequestTargetParseResult parse_request_target(std::string_view method,
                                                             std::string_view raw_target);

}  // namespace cppapplab::http
