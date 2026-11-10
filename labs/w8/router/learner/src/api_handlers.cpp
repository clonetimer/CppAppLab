#include "api_handlers.hpp"

#include <stdexcept>

namespace cppapplab::http {

HttpResponse hello_handler(const HttpRequest& request) {
    // TODO(W8-API-1): return the course greeting with text/plain; charset=utf-8.
    (void)request;
    throw std::logic_error("TODO: implement hello_handler");
}

HttpResponse echo_handler(const HttpRequest& request) {
    // TODO(W8-API-2): preserve every request body byte, including NUL and UTF-8.
    (void)request;
    throw std::logic_error("TODO: implement echo_handler");
}

HttpResponse health_handler(const HttpRequest& request) {
    // TODO(W8-API-3): return the fixed JSON health document and application/json.
    (void)request;
    throw std::logic_error("TODO: implement health_handler");
}

Router make_default_router() {
    // TODO(W8-API-4): register GET /hello, POST /echo and GET /health.
    throw std::logic_error("TODO: register the three dynamic endpoints");
}

}  // namespace cppapplab::http
