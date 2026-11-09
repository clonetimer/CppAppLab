#include "http_message.hpp"

#include <stdexcept>

namespace cppapplab::http {

bool parse_request_line(std::string_view line, HttpRequest& request) {
    // TODO(W8-HTTP-1): parse exactly METHOD target VERSION.
    // Reject missing fields, extra tokens and malformed request lines.
    (void)line;
    (void)request;
    return false;
}

std::string serialize_response(const HttpResponse& response) {
    // TODO(W8-HTTP-2): serialize an HTTP/1.1 status line, headers, CRLFCRLF and body.
    // Content-Length must be the byte length of the final body.
    // Force Connection: close for this first protocol exercise.
    (void)response;
    throw std::logic_error("TODO: implement HTTP response serialization");
}

}  // namespace cppapplab::http
