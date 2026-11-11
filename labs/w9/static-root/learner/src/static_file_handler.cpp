#include "static_file_handler.hpp"

#include "http_request_target.hpp"

#include <utility>

namespace cppapplab::http {

struct StaticFileHandler::DirectoryRoot {};

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root)
    : StaticFileHandler(std::move(document_root), std::numeric_limits<std::size_t>::max()) {}

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root,
                                     std::size_t max_file_bytes)
    : max_file_bytes_(max_file_bytes) {
    // TODO(student): open and retain the document-root directory descriptor safely.
    (void)document_root;
}

HttpResponse StaticFileHandler::handle(const HttpRequest& request) const {
    // TODO(student): parse the request target and open every path component with openat/O_NOFOLLOW.
    (void)request;
    HttpResponse response;
    response.status_code = 501;
    response.reason_phrase = "Not Implemented";
    response.headers.emplace("Connection", "close");
    response.body = "501 Not Implemented\n";
    return response;
}

}  // namespace cppapplab::http
