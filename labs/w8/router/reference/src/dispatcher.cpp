#include "dispatcher.hpp"

namespace cppapplab::http {

HttpResponse dispatch_request(const HttpRequest& request, const Router& router,
                              const StaticFileHandler& static_files) {
    const std::string path = request.target.substr(0U, request.target.find('?'));
    // A known dynamic path owns both success and method-mismatch responses.
    // Never use a handler's HTTP status to decide whether to fall back to disk.
    if (router.has_path(path)) {
        return router.handle(request);
    }
    return static_files.handle(request);
}

}  // namespace cppapplab::http
