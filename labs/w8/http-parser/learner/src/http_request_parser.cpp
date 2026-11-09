#include "http_request_parser.hpp"

#include <stdexcept>

namespace cppapplab::http {

ParseResult HttpRequestParser::parse(std::string_view buffer) const {
    // TODO(W8-PARSER-1): find the CRLFCRLF header terminator.
    // TODO(W8-PARSER-2): parse request line and case-insensitive headers.
    // TODO(W8-PARSER-3): require Host for HTTP/1.1 and reject Transfer-Encoding.
    // TODO(W8-PARSER-4): strictly parse Content-Length and enforce 16 KiB / 1 MiB limits.
    // TODO(W8-PARSER-5): distinguish NeedMoreData, Complete and Error.
    // TODO(W8-PARSER-6): report consumed bytes so pipelined requests remain in the buffer.
    (void)buffer;
    throw std::logic_error("TODO: implement incremental HTTP request parsing");
}

}  // namespace cppapplab::http
