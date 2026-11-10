#include "router.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace cppapplab::http {
namespace {

bool valid_method(std::string_view method) {
    constexpr std::string_view punctuation = "!#$%&'*+-.^_`|~";
    if (method.empty()) {
        return false;
    }
    for (const unsigned char c : method) {
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        if (!alpha && !digit && punctuation.find(static_cast<char>(c)) == std::string_view::npos) {
            return false;
        }
    }
    return true;
}

bool valid_path(std::string_view path) {
    if (path.empty() || path.front() != '/') {
        return false;
    }
    for (const unsigned char c : path) {
        if (c <= 0x20U || c >= 0x7fU || c == '?' || c == '#' || c == '\\') {
            return false;
        }
    }
    return true;
}

HttpResponse error_response(int status, std::string reason) {
    HttpResponse response;
    response.status_code = status;
    response.reason_phrase = std::move(reason);
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.body = std::to_string(status) + " " + response.reason_phrase + "\n";
    return response;
}

}  // namespace

void Router::add_route(std::string method, std::string path, Handler handler) {
    if (!valid_method(method) || !valid_path(path) || !handler) {
        throw std::invalid_argument("route requires a method token, absolute path and handler");
    }
    routes_[std::move(path)].insert_or_assign(std::move(method), std::move(handler));
}

bool Router::has_route(const std::string& method, const std::string& path) const {
    const auto found = routes_.find(path);
    return found != routes_.end() && found->second.find(method) != found->second.end();
}

bool Router::has_path(const std::string& path) const {
    return routes_.find(path) != routes_.end();
}

HttpResponse Router::handle(const HttpRequest& request) const {
    const std::string path = request.target.substr(0U, request.target.find('?'));
    const auto found = routes_.find(path);
    if (found == routes_.end()) {
        return error_response(404, "Not Found");
    }
    const auto route = found->second.find(request.method);
    if (route != found->second.end()) {
        return route->second(request);
    }

    std::vector<std::string> methods;
    methods.reserve(found->second.size());
    for (const auto& entry : found->second) {
        methods.push_back(entry.first);
    }
    std::sort(methods.begin(), methods.end());
    std::string allow;
    for (const auto& method : methods) {
        if (!allow.empty()) {
            allow += ", ";
        }
        allow += method;
    }
    HttpResponse response = error_response(405, "Method Not Allowed");
    response.headers.emplace("Allow", std::move(allow));
    return response;
}

}  // namespace cppapplab::http
