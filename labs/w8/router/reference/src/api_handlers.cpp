#include "api_handlers.hpp"

namespace cppapplab::http {

HttpResponse hello_handler(const HttpRequest&) {
    HttpResponse response;
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.body = "Hello from C++ HTTP Server!\n";
    return response;
}

HttpResponse echo_handler(const HttpRequest& request) {
    HttpResponse response;
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.body = request.body;
    return response;
}

HttpResponse health_handler(const HttpRequest&) {
    HttpResponse response;
    response.headers.emplace("Content-Type", "application/json");
    response.body = R"({"status":"ok"})";
    return response;
}

Router make_default_router() {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    router.add_route("POST", "/echo", echo_handler);
    router.add_route("GET", "/health", health_handler);
    return router;
}

}  // namespace cppapplab::http
