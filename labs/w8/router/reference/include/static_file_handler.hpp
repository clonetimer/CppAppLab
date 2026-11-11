#pragma once

#include "http_message.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>

namespace cppapplab::http {

class StaticFileHandler {
   public:
    explicit StaticFileHandler(std::filesystem::path document_root);
    StaticFileHandler(std::filesystem::path document_root, std::size_t max_file_bytes);

    [[nodiscard]] HttpResponse handle(const HttpRequest& request) const;

   private:
    std::filesystem::path document_root_;
    std::size_t max_file_bytes_;
};

}  // namespace cppapplab::http
