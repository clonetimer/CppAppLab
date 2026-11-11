#include "static_file_handler.hpp"

#include "http_request_target.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace cppapplab::http {
namespace {

class UniqueFd {
   public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    ~UniqueFd() {
        reset();
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

   private:
    void reset() noexcept {
        if (fd_ >= 0) {
            (void)::close(fd_);
            fd_ = -1;
        }
    }

    int fd_{-1};
};

enum class FileReadStatus { Success, NotFound, TooLarge };

struct FileReadResult {
    FileReadStatus status{FileReadStatus::NotFound};
    std::string contents;
};

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

[[nodiscard]] HttpResponse text_response(int status, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status;
    response.reason_phrase = std::move(reason);
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.headers.emplace("Connection", "close");
    response.body = std::move(body);
    return response;
}

[[nodiscard]] std::string resolve_document_path(std::string_view decoded_path) {
    if (decoded_path == "/") {
        return "/index.html";
    }
    std::string resolved(decoded_path);
    if (resolved.back() == '/') {
        resolved += "index.html";
    }
    return resolved;
}

[[nodiscard]] UniqueFd open_file_beneath(int root_fd, std::string_view decoded_path) {
    if (decoded_path.empty() || decoded_path.front() != '/') {
        return UniqueFd{};
    }

    const std::string relative_path = resolve_document_path(decoded_path);

    UniqueFd current_directory;
    int current_fd = root_fd;
    std::size_t cursor = 1U;

    while (cursor <= relative_path.size()) {
        const std::size_t separator = relative_path.find('/', cursor);
        const std::size_t segment_end =
            separator == std::string::npos ? relative_path.size() : separator;
        const std::string_view segment(relative_path.data() + cursor, segment_end - cursor);

        if (segment.empty() || segment == "." || segment == ".." ||
            segment.find('\\') != std::string_view::npos) {
            return UniqueFd{};
        }

        const std::string component(segment);
        if (separator == std::string::npos) {
            return UniqueFd(::openat(current_fd, component.c_str(),
                                     O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        }

        UniqueFd next_directory(::openat(current_fd, component.c_str(),
                                         O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        if (next_directory.get() < 0) {
            return UniqueFd{};
        }
        current_directory = std::move(next_directory);
        current_fd = current_directory.get();
        cursor = separator + 1U;
    }

    return UniqueFd{};
}

[[nodiscard]] FileReadResult read_regular_file(int fd, std::size_t max_file_bytes) {
    struct stat metadata {};
    if (::fstat(fd, &metadata) < 0 || !S_ISREG(metadata.st_mode) || metadata.st_size < 0) {
        return {};
    }

    const std::uintmax_t declared_size = static_cast<std::uintmax_t>(metadata.st_size);
    if (declared_size > max_file_bytes) {
        return {FileReadStatus::TooLarge, {}};
    }

    std::string contents;
    contents.reserve(static_cast<std::size_t>(declared_size));
    char buffer[8192];

    for (;;) {
        const ssize_t received = ::read(fd, buffer, sizeof(buffer));
        if (received > 0) {
            const std::size_t count = static_cast<std::size_t>(received);
            if (contents.size() > max_file_bytes || count > max_file_bytes - contents.size()) {
                return {FileReadStatus::TooLarge, {}};
            }
            contents.append(buffer, count);
            continue;
        }
        if (received == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        return {};
    }

    return {FileReadStatus::Success, std::move(contents)};
}

}  // namespace

struct StaticFileHandler::DirectoryRoot {
    explicit DirectoryRoot(const std::filesystem::path& path) {
        fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "cannot open static document root");
        }

        struct stat metadata {};
        if (::fstat(fd, &metadata) < 0) {
            const int error = errno;
            (void)::close(fd);
            fd = -1;
            throw std::system_error(error, std::generic_category(),
                                    "cannot inspect static document root");
        }
        if (!S_ISDIR(metadata.st_mode)) {
            (void)::close(fd);
            fd = -1;
            throw std::invalid_argument("static document root must be a directory");
        }
    }

    ~DirectoryRoot() {
        if (fd >= 0) {
            (void)::close(fd);
        }
    }

    int fd{-1};
};

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root)
    : StaticFileHandler(std::move(document_root), std::numeric_limits<std::size_t>::max()) {}

StaticFileHandler::StaticFileHandler(std::filesystem::path document_root,
                                     std::size_t max_file_bytes)
    : root_(std::make_shared<DirectoryRoot>(document_root)), max_file_bytes_(max_file_bytes) {}

HttpResponse StaticFileHandler::handle(const HttpRequest& request) const {
    if (request.method != "GET") {
        HttpResponse response =
            text_response(405, "Method Not Allowed", "405 Method Not Allowed\n");
        response.headers.emplace("Allow", "GET");
        return response;
    }

    const RequestTargetParseResult parsed = parse_request_target(request.method, request.target);
    if (parsed.status != RequestTargetStatus::Complete || parsed.target.asterisk_form) {
        return text_response(400, "Bad Request", "400 Bad Request\n");
    }

    const std::string resolved_path = resolve_document_path(parsed.target.path);
    const UniqueFd file = open_file_beneath(root_->fd, resolved_path);
    if (file.get() < 0) {
        return text_response(404, "Not Found", "404 Not Found\n");
    }

    FileReadResult contents = read_regular_file(file.get(), max_file_bytes_);
    if (contents.status == FileReadStatus::TooLarge) {
        return text_response(413, "Content Too Large", "413 Content Too Large\n");
    }
    if (contents.status != FileReadStatus::Success) {
        return text_response(404, "Not Found", "404 Not Found\n");
    }

    HttpResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.headers.emplace("Content-Type", content_type(resolved_path));
    response.headers.emplace("Connection", "close");
    response.body = std::move(contents.contents);
    return response;
}

}  // namespace cppapplab::http
