#pragma once

#include "http_message.hpp"

#include <filesystem>

namespace cppapplab::http {

class StaticFileHandler {
   public:
    explicit StaticFileHandler(std::filesystem::path document_root);

    [[nodiscard]] HttpResponse handle(const HttpRequest& request) const;

   private:
    std::filesystem::path document_root_;
};

}  // namespace cppapplab::http
