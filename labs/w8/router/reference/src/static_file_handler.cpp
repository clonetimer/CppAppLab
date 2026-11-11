#include "static_file_handler.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace cppapplab::http {
namespace {

struct FileReadResult {
    enum class Status { Success, Unreadable, TooLarge };

    Status status{Status::Unreadable};
    std::string contents;
};

[[nodiscard]] FileReadResult read_file(const std::filesystem::path& path,
                                       std::size_t max_file_bytes) {
    std::error_code size_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, size_error);
    if (!size_error && file_size > max_file_bytes) {
        return {FileReadResult::Status::TooLarge, {}};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::string contents;
    if (!size_error) {
        contents.reserve(static_cast<std::size_t>(file_size));
    }

    char buffer[8192];
    for (;;) {
        file.read(buffer, static_cast<std::streamsize>(sizeof(buffer)));
        const std::streamsize received = file.gcount();
        if (received > 0) {
            const std::size_t count = static_cast<std::size_t>(received);
            if (contents.size() > max_file_bytes || count > max_file_bytes - contents.size()) {
                return {FileReadResult::Status::TooLarge, {}};
            }
            contents.append(buffer, count);
        }

        if (file.bad()) {
            return {};
        }
        if (file.eof()) {
            break;
        }
        if (file.fail()) {
            return {};
        }
    }

    return {FileReadResult::Status::Success, std::move(contents)};
}

[[nodiscard]] std::string content_type(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension == ".html") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".js") {
        return "application/javascript";
    }
    if (extension == ".txt") {
        return "text/plain; charset=utf-8";
    }
    if (extension == ".json") {
        return "application/json";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

[[nodiscard]] bool is_safe_target(std::string_view target) {
    if (target.empty() || target.front() != '/') {
        return false;
    }
    if (target.find("..") != std::string_view::npos) {
        return false;
    }
    if (target.find('\\') != std::string_view::npos) {
        return false;
    }
    return true;
}

[[nodiscard]] HttpResponse text_response(int status, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status;
    response.reason_phrase = std::move(reason);
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.headers.emplace("Connection", "close");
    response.body = std::move(body);
    return response;
}

}  // namespace

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root)
    : StaticFileHandler(std::move(document_root), std::numeric_limits<std::size_t>::max()) {}

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root,
                                     std::size_t max_file_bytes)
    : document_root_(std::move(document_root)), max_file_bytes_(max_file_bytes) {}

HttpResponse StaticFileHandler::handle(const HttpRequest& request) const {
    if (request.method != "GET") {
        HttpResponse response =
            text_response(405, "Method Not Allowed", "405 Method Not Allowed\n");
        response.headers.emplace("Allow", "GET");
        return response;
    }

    std::string target = request.target;
    const std::size_t query = target.find('?');
    if (query != std::string::npos) {
        target.erase(query);
    }

    if (!is_safe_target(target)) {
        return text_response(400, "Bad Request", "400 Bad Request\n");
    }
    if (target == "/") {
        target = "/index.html";
    }

    const std::filesystem::path relative = std::filesystem::path(target).relative_path();
    const std::filesystem::path file_path = document_root_ / relative;
    FileReadResult contents = read_file(file_path, max_file_bytes_);
    if (contents.status == FileReadResult::Status::TooLarge) {
        return text_response(413, "Content Too Large", "413 Content Too Large\n");
    }
    if (contents.status != FileReadResult::Status::Success) {
        return text_response(404, "Not Found", "404 Not Found\n");
    }

    HttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.headers.emplace("Content-Type", content_type(file_path));
    response.headers.emplace("Connection", "close");
    response.body = std::move(contents.contents);
    return response;
}

}  // namespace cppapplab::http
