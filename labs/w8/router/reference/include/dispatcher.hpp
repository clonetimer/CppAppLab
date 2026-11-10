#pragma once

#include "router.hpp"
#include "static_file_handler.hpp"

namespace cppapplab::http {

[[nodiscard]] HttpResponse dispatch_request(const HttpRequest& request, const Router& router,
                                            const StaticFileHandler& static_files);

}  // namespace cppapplab::http
