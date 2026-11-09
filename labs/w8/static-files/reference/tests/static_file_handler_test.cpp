#include "static_file_handler.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace cppapplab::http;

namespace {

class TemporaryDocumentRoot {
   public:
    TemporaryDocumentRoot() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-static-files-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDocumentRoot() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }
    void write(const std::filesystem::path& relative, const std::string& contents) const {
        const auto target = path_ / relative;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

   private:
    std::filesystem::path path_;
};

[[nodiscard]] HttpRequest make_request(std::string method, std::string target) {
    HttpRequest request;
    request.method = std::move(method);
    request.target = std::move(target);
    return request;
}

}  // namespace

TEST(StaticFileHandlerReferenceTest, MapsRootToIndexHtml) {
    TemporaryDocumentRoot root;
    root.write("index.html", "<h1>home</h1>");
    const HttpResponse response = StaticFileHandler(root.path()).handle(make_request("GET", "/"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "<h1>home</h1>");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/html; charset=utf-8");
}

TEST(StaticFileHandlerReferenceTest, SelectsCssMimeType) {
    TemporaryDocumentRoot root;
    root.write("style.css", "body{}");
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/style.css"));
    EXPECT_EQ(response.headers.at("Content-Type"), "text/css; charset=utf-8");
}

TEST(StaticFileHandlerReferenceTest, SelectsJavaScriptMimeType) {
    TemporaryDocumentRoot root;
    root.write("app.js", "console.log(1)");
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/app.js"));
    EXPECT_EQ(response.headers.at("Content-Type"), "application/javascript");
}

TEST(StaticFileHandlerReferenceTest, SelectsPlainTextMimeType) {
    TemporaryDocumentRoot root;
    root.write("hello.txt", "hello");
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/hello.txt"));
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain; charset=utf-8");
}

TEST(StaticFileHandlerReferenceTest, SelectsJsonMimeType) {
    TemporaryDocumentRoot root;
    root.write("data.json", "{}");
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/data.json"));
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST(StaticFileHandlerReferenceTest, ReturnsNotFoundForMissingFile) {
    TemporaryDocumentRoot root;
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/missing.txt"));
    EXPECT_EQ(response.status_code, 404);
    EXPECT_EQ(response.reason_phrase, "Not Found");
}

TEST(StaticFileHandlerReferenceTest, RejectsNonGetMethod) {
    TemporaryDocumentRoot root;
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("POST", "/"));
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.headers.at("Allow"), "GET");
}

TEST(StaticFileHandlerReferenceTest, RejectsPathTraversal) {
    TemporaryDocumentRoot root;
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/../secret"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(StaticFileHandlerReferenceTest, RemovesQueryBeforeFileLookup) {
    TemporaryDocumentRoot root;
    root.write("hello.txt", "hello");
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/hello.txt?x=1"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "hello");
}

TEST(StaticFileHandlerReferenceTest, PreservesBinaryFileBytes) {
    TemporaryDocumentRoot root;
    const std::string binary("\0\x01\x7f\xff", 4U);
    root.write("asset.bin", binary);
    const auto response =
        StaticFileHandler(root.path()).handle(make_request("GET", "/asset.bin"));
    EXPECT_EQ(response.body, binary);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/octet-stream");
}
