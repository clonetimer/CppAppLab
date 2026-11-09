#include "static_file_handler.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

using namespace cppapplab::http;

namespace {

class TemporaryDocumentRoot {
   public:
    TemporaryDocumentRoot() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-static-acceptance-" + std::to_string(stamp));
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
        ASSERT_TRUE(output.good());
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        ASSERT_TRUE(output.good());
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

TEST(StaticFileAcceptanceTest, MapsRootToIndexHtml) {
    TemporaryDocumentRoot root;
    root.write("index.html", "<h1>home</h1>");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "<h1>home</h1>");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/html; charset=utf-8");
}

TEST(StaticFileAcceptanceTest, ReadsHtmlFileBytes) {
    TemporaryDocumentRoot root;
    root.write("page.html", "<p>hello</p>");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/page.html"));
    EXPECT_EQ(response.body, "<p>hello</p>");
}

TEST(StaticFileAcceptanceTest, SelectsCssMimeType) {
    TemporaryDocumentRoot root;
    root.write("style.css", "body{}");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/style.css"));
    EXPECT_EQ(response.headers.at("Content-Type"), "text/css; charset=utf-8");
}

TEST(StaticFileAcceptanceTest, SelectsJavaScriptMimeType) {
    TemporaryDocumentRoot root;
    root.write("app.js", "console.log(1)");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/app.js"));
    EXPECT_EQ(response.headers.at("Content-Type"), "application/javascript");
}

TEST(StaticFileAcceptanceTest, SelectsPlainTextMimeType) {
    TemporaryDocumentRoot root;
    root.write("hello.txt", "hello");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/hello.txt"));
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain; charset=utf-8");
}

TEST(StaticFileAcceptanceTest, SelectsJsonMimeType) {
    TemporaryDocumentRoot root;
    root.write("data.json", "{}");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/data.json"));
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST(StaticFileAcceptanceTest, SelectsPngMimeType) {
    TemporaryDocumentRoot root;
    root.write("pixel.png", "png");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/pixel.png"));
    EXPECT_EQ(response.headers.at("Content-Type"), "image/png");
}

TEST(StaticFileAcceptanceTest, SelectsJpegMimeType) {
    TemporaryDocumentRoot root;
    root.write("photo.jpg", "jpeg");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/photo.jpg"));
    EXPECT_EQ(response.headers.at("Content-Type"), "image/jpeg");
}

TEST(StaticFileAcceptanceTest, SelectsSvgMimeType) {
    TemporaryDocumentRoot root;
    root.write("icon.svg", "<svg/>");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/icon.svg"));
    EXPECT_EQ(response.headers.at("Content-Type"), "image/svg+xml");
}

TEST(StaticFileAcceptanceTest, UsesBinaryMimeTypeForUnknownExtension) {
    TemporaryDocumentRoot root;
    root.write("asset.bin", "bytes");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/asset.bin"));
    EXPECT_EQ(response.headers.at("Content-Type"), "application/octet-stream");
}

TEST(StaticFileAcceptanceTest, Returns404ForMissingFile) {
    TemporaryDocumentRoot root;
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/missing.txt"));
    EXPECT_EQ(response.status_code, 404);
    EXPECT_EQ(response.reason_phrase, "Not Found");
}

TEST(StaticFileAcceptanceTest, Returns405AndAllowForNonGet) {
    TemporaryDocumentRoot root;
    const auto response = StaticFileHandler(root.path()).handle(make_request("POST", "/"));
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.reason_phrase, "Method Not Allowed");
    EXPECT_EQ(response.headers.at("Allow"), "GET");
}

TEST(StaticFileAcceptanceTest, RejectsRelativeTarget) {
    TemporaryDocumentRoot root;
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "index.html"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(StaticFileAcceptanceTest, RejectsDotDotTraversal) {
    TemporaryDocumentRoot root;
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/../secret"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(StaticFileAcceptanceTest, RejectsBackslashTarget) {
    TemporaryDocumentRoot root;
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/..\\secret"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(StaticFileAcceptanceTest, RemovesQueryStringBeforeLookup) {
    TemporaryDocumentRoot root;
    root.write("hello.txt", "hello");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/hello.txt?x=1"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "hello");
}

TEST(StaticFileAcceptanceTest, QueryValueDoesNotTriggerPathTraversalRejection) {
    TemporaryDocumentRoot root;
    root.write("hello.txt", "hello");
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/hello.txt?next=.."));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "hello");
}

TEST(StaticFileAcceptanceTest, PreservesBinaryFileBytes) {
    TemporaryDocumentRoot root;
    const std::string binary("\0\x01\x7f\xff", 4U);
    root.write("asset.bin", binary);
    const auto response = StaticFileHandler(root.path()).handle(make_request("GET", "/asset.bin"));
    EXPECT_EQ(response.body, binary);
}

TEST(StaticFileAcceptanceTest, SerializerComputesContentLengthFromUtf8Bytes) {
    HttpResponse response;
    response.body = "你好";
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Content-Length: 6\r\n"), std::string::npos);
}

TEST(StaticFileAcceptanceTest, SerializerAlwaysEmitsConnectionClose) {
    HttpResponse response;
    response.headers.emplace("Connection", "keep-alive");
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("keep-alive"), std::string::npos);
}
