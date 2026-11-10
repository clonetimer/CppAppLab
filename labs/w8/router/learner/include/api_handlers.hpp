#pragma once

#include "router.hpp"

namespace cppapplab::http {

[[nodiscard]] HttpResponse hello_handler(const HttpRequest& request);
[[nodiscard]] HttpResponse echo_handler(const HttpRequest& request);
[[nodiscard]] HttpResponse health_handler(const HttpRequest& request);
[[nodiscard]] Router make_default_router();

}  // namespace cppapplab::http
