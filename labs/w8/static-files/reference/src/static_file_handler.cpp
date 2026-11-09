#include "static_file_handler.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace cppapplab::http {
namespace {

[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return std::nullopt;
    }
    return buffer.str();
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
    : document_root_(std::move(document_root)) {}

HttpResponse StaticFileHandler::handle(const HttpRequest& request) const {
    if (request.method != "GET") {
        HttpResponse response =
            text_response(405, "Method Not Allowed", "405 Method Not Allowed\n");
        response.headers.emplace("Allow", "GET");
        return response;
    }

    if (!is_safe_target(request.target)) {
        return text_response(400, "Bad Request", "400 Bad Request\n");
    }

    std::string target = request.target;
    const std::size_t query = target.find('?');
    if (query != std::string::npos) {
        target.erase(query);
    }
    if (target == "/") {
        target = "/index.html";
    }

    const std::filesystem::path relative = std::filesystem::path(target).relative_path();
    const std::filesystem::path file_path = document_root_ / relative;
    auto contents = read_file(file_path);
    if (!contents.has_value()) {
        return text_response(404, "Not Found", "404 Not Found\n");
    }

    HttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.headers.emplace("Content-Type", content_type(file_path));
    response.headers.emplace("Connection", "close");
    response.body = std::move(*contents);
    return response;
}

}  // namespace cppapplab::http
