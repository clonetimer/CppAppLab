#include "static_file_handler.hpp"

#include <stdexcept>
#include <utility>

namespace cppapplab::http {

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root)
    : document_root_(std::move(document_root)) {}

HttpResponse StaticFileHandler::handle(const HttpRequest& request) const {
    // TODO(W8-STATIC-1): allow GET only; return 405 with Allow: GET otherwise.
    // TODO(W8-STATIC-2): reject unsafe targets (non-rooted, "..", backslash).
    // TODO(W8-STATIC-3): strip query string and map "/" to "/index.html".
    // TODO(W8-STATIC-4): resolve the relative path beneath document_root_.
    // TODO(W8-STATIC-5): read file bytes in binary mode and return 404 if missing.
    // TODO(W8-STATIC-6): choose MIME type by extension and build HttpResponse.
    (void)request;
    throw std::logic_error("TODO: implement StaticFileHandler::handle");
}

}  // namespace cppapplab::http
