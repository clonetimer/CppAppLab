#include "dispatcher.hpp"

#include <stdexcept>

namespace cppapplab::http {

HttpResponse dispatch_request(const HttpRequest& request, const Router& router,
                              const StaticFileHandler& static_files) {
    // TODO(W8-DISPATCH-1): a known dynamic path owns all of its responses, including 405/404.
    // Only unknown dynamic paths fall back to StaticFileHandler.
    (void)request;
    (void)router;
    (void)static_files;
    throw std::logic_error("TODO: implement dynamic/static request dispatch");
}

}  // namespace cppapplab::http
