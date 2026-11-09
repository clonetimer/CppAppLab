#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace cppapplab::http {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code{200};
    std::string reason_phrase{"OK"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

[[nodiscard]] bool parse_request_line(std::string_view line, HttpRequest& request);
[[nodiscard]] std::string serialize_response(const HttpResponse& response);

}  // namespace cppapplab::http
