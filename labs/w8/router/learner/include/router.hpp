#pragma once

#include "http_message.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace cppapplab::http {

class Router {
   public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    void add_route(std::string method, std::string path, Handler handler);
    [[nodiscard]] bool has_route(const std::string& method, const std::string& path) const;
    [[nodiscard]] bool has_path(const std::string& path) const;
    [[nodiscard]] HttpResponse handle(const HttpRequest& request) const;

   private:
    std::unordered_map<std::string, std::unordered_map<std::string, Handler>> routes_;
};

}  // namespace cppapplab::http
