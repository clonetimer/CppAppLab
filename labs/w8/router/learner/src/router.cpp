#include "router.hpp"

#include <stdexcept>

namespace cppapplab::http {

void Router::add_route(std::string method, std::string path, Handler handler) {
    // TODO(W8-ROUTER-1): validate registration before replacing the method/path entry.
    (void)method;
    (void)path;
    (void)handler;
    throw std::logic_error("TODO: implement route registration");
}

bool Router::has_route(const std::string& method, const std::string& path) const {
    // TODO(W8-ROUTER-2): match both the exact method and path.
    (void)method;
    (void)path;
    return false;
}

bool Router::has_path(const std::string& path) const {
    // TODO(W8-ROUTER-3): detect a dynamic path independently of the method.
    (void)path;
    return false;
}

HttpResponse Router::handle(const HttpRequest& request) const {
    // TODO(W8-ROUTER-4): strip query for matching, but pass the original request to the callback.
    // Return 404 for absent paths, or 405 with sorted Allow for a method mismatch.
    (void)request;
    throw std::logic_error("TODO: implement dynamic request handling");
}

}  // namespace cppapplab::http
