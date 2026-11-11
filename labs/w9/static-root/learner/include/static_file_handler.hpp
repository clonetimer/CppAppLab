#pragma once

#include "http_message.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>

namespace cppapplab::http {

class StaticFileHandler {
   public:
    explicit StaticFileHandler(std::filesystem::path document_root);
    StaticFileHandler(std::filesystem::path document_root, std::size_t max_file_bytes);

    [[nodiscard]] HttpResponse handle(const HttpRequest& request) const;

   private:
    struct DirectoryRoot;
    std::shared_ptr<DirectoryRoot> root_;
    std::size_t max_file_bytes_;
};

}  // namespace cppapplab::http
