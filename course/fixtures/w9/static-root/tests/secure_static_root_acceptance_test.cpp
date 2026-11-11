#include "static_file_handler.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <sys/stat.h>

using namespace cppapplab::http;

namespace {

class TemporaryTree {
   public:
    TemporaryTree() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        base_ = std::filesystem::temp_directory_path() /
                ("cppapplab-secure-static-root-" + std::to_string(stamp));
        root_ = base_ / "www";
        std::filesystem::create_directories(root_);
    }

    ~TemporaryTree() {
        std::error_code ignored;
        std::filesystem::remove_all(base_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& base() const noexcept {
        return base_;
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    void write(const std::filesystem::path& relative, const std::string& contents) const {
        const auto target = root_ / relative;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create fixture file");
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) {
            throw std::runtime_error("cannot write fixture file");
        }
    }

   private:
    std::filesystem::path base_;
    std::filesystem::path root_;
};

[[nodiscard]] HttpRequest make_request(std::string method, std::string target) {
    HttpRequest request;
    request.method = std::move(method);
    request.target = std::move(target);
    request.version = "HTTP/1.1";
    return request;
}

}  // namespace

TEST(SecureStaticRootAcceptanceTest, MapsRootToIndexHtml) {
    TemporaryTree tree;
    tree.write("index.html", "<h1>home</h1>");
    const HttpResponse response = StaticFileHandler(tree.root()).handle(make_request("GET", "/"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "<h1>home</h1>");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/html; charset=utf-8");
}

TEST(SecureStaticRootAcceptanceTest, MapsTrailingSlashToNestedIndexHtml) {
    TemporaryTree tree;
    tree.write("docs/index.html", "documentation");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/docs/"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "documentation");
}

TEST(SecureStaticRootAcceptanceTest, ServesNestedRegularFile) {
    TemporaryTree tree;
    tree.write("assets/css/site.css", "body { color: black; }");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/assets/css/site.css"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "body { color: black; }");
}

TEST(SecureStaticRootAcceptanceTest, DecodesPathBeforeFilesystemLookup) {
    TemporaryTree tree;
    tree.write("hello world.txt", "hello");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/hello%20world.txt"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "hello");
}

TEST(SecureStaticRootAcceptanceTest, IgnoresQueryStringForFileLookup) {
    TemporaryTree tree;
    tree.write("hello.txt", "hello");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/hello.txt?download=1"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "hello");
}

TEST(SecureStaticRootAcceptanceTest, PreservesBinaryFileBytes) {
    TemporaryTree tree;
    const std::string binary("\0\x01\x7f\xff", 4U);
    tree.write("asset.bin", binary);
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/asset.bin"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, binary);
}

TEST(SecureStaticRootAcceptanceTest, SelectsContentTypeFromResolvedPath) {
    TemporaryTree tree;
    tree.write("image.png", "png");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/image.png"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.headers.at("Content-Type"), "image/png");
}

TEST(SecureStaticRootAcceptanceTest, UsesBinaryContentTypeForUnknownExtension) {
    TemporaryTree tree;
    tree.write("archive.data", "bytes");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/archive.data"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/octet-stream");
}

TEST(SecureStaticRootAcceptanceTest, ServesEmptyRegularFile) {
    TemporaryTree tree;
    tree.write("empty.txt", "");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/empty.txt"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_TRUE(response.body.empty());
}

TEST(SecureStaticRootAcceptanceTest, Returns404ForMissingFile) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/missing.txt"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, DoesNotServeDirectoryWithoutTrailingSlash) {
    TemporaryTree tree;
    tree.write("docs/index.html", "documentation");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/docs"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, Returns405AndAllowForNonGetMethod) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("POST", "/index.html"));
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.headers.at("Allow"), "GET");
}

TEST(SecureStaticRootAcceptanceTest, RejectsMalformedPercentEncoding) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/bad%GG.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsEncodedSlash) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/assets%2fsecret.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsEncodedBackslash) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/assets%5csecret.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsDotDotTraversal) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/../secret.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsPercentEncodedDotDotTraversal) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/%2e%2e/secret.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsAbsoluteFormRequestTarget) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "http://host/secret.txt"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsAsteriskFormForGet) {
    TemporaryTree tree;
    const HttpResponse response = StaticFileHandler(tree.root()).handle(make_request("GET", "*"));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsRepeatedPathSeparators) {
    TemporaryTree tree;
    tree.write("docs/index.html", "documentation");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/docs//index.html"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, RejectsFinalComponentSymlink) {
    TemporaryTree tree;
    const auto outside = tree.base() / "outside.txt";
    {
        std::ofstream output(outside);
        output << "outside";
    }
    std::filesystem::create_symlink(outside, tree.root() / "link.txt");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/link.txt"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, RejectsIntermediateDirectorySymlink) {
    TemporaryTree tree;
    const auto outside = tree.base() / "outside";
    std::filesystem::create_directories(outside);
    {
        std::ofstream output(outside / "secret.txt");
        output << "outside";
    }
    std::filesystem::create_directory_symlink(outside, tree.root() / "escape");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/escape/secret.txt"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, RejectsFifoWithoutBlocking) {
    TemporaryTree tree;
    const auto fifo = tree.root() / "events";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/events"));
    EXPECT_EQ(response.status_code, 404);
}

TEST(SecureStaticRootAcceptanceTest, AcceptsFileExactlyAtConfiguredSizeLimit) {
    TemporaryTree tree;
    tree.write("small.txt", "1234");
    const HttpResponse response =
        StaticFileHandler(tree.root(), 4U).handle(make_request("GET", "/small.txt"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "1234");
}

TEST(SecureStaticRootAcceptanceTest, Returns413ForFileAboveConfiguredSizeLimit) {
    TemporaryTree tree;
    tree.write("large.txt", "12345");
    const HttpResponse response =
        StaticFileHandler(tree.root(), 4U).handle(make_request("GET", "/large.txt"));
    EXPECT_EQ(response.status_code, 413);
}

TEST(SecureStaticRootAcceptanceTest, KeepsRootDirectoryDescriptorAcrossRename) {
    TemporaryTree tree;
    tree.write("index.html", "stable root");
    StaticFileHandler handler(tree.root());
    const auto moved = tree.base() / "renamed-root";
    std::filesystem::rename(tree.root(), moved);

    const HttpResponse response = handler.handle(make_request("GET", "/index.html"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "stable root");
}

TEST(SecureStaticRootAcceptanceTest, SharedHandlerCopyRetainsRootDescriptorLifetime) {
    TemporaryTree tree;
    tree.write("index.html", "shared root");
    std::unique_ptr<StaticFileHandler> copy;
    {
        StaticFileHandler original(tree.root());
        copy = std::make_unique<StaticFileHandler>(original);
    }

    const HttpResponse response = copy->handle(make_request("GET", "/index.html"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "shared root");
}

TEST(SecureStaticRootAcceptanceTest, RejectsSymlinkAsDocumentRoot) {
    TemporaryTree tree;
    const auto link = tree.base() / "root-link";
    std::filesystem::create_directory_symlink(tree.root(), link);
    EXPECT_THROW(StaticFileHandler(link), std::system_error);
}

TEST(SecureStaticRootAcceptanceTest, RejectsMissingDocumentRoot) {
    TemporaryTree tree;
    EXPECT_THROW(StaticFileHandler(tree.base() / "missing"), std::system_error);
}

TEST(SecureStaticRootAcceptanceTest, RejectsRegularFileAsDocumentRoot) {
    TemporaryTree tree;
    const auto file = tree.base() / "not-a-directory";
    {
        std::ofstream output(file);
        output << "not a directory";
    }
    EXPECT_THROW(StaticFileHandler(file), std::system_error);
}

TEST(SecureStaticRootAcceptanceTest, DoesNotDecodePercentEncodingTwice) {
    TemporaryTree tree;
    tree.write("%2e%2e/inside.txt", "literal percent path");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/%252e%252e/inside.txt"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "literal percent path");
}

TEST(SecureStaticRootAcceptanceTest, KeepsEncodedPercentInFilenameLiteral) {
    TemporaryTree tree;
    tree.write("value%2Fname.txt", "literal percent sequence");
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("GET", "/value%252Fname.txt"));
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "literal percent sequence");
}

TEST(SecureStaticRootAcceptanceTest, ReturnsBadRequestForEmptyTarget) {
    TemporaryTree tree;
    const HttpResponse response = StaticFileHandler(tree.root()).handle(make_request("GET", ""));
    EXPECT_EQ(response.status_code, 400);
}

TEST(SecureStaticRootAcceptanceTest, RejectsNonGetMethodBeforeFilesystemLookup) {
    TemporaryTree tree;
    const HttpResponse response =
        StaticFileHandler(tree.root()).handle(make_request("G ET", "/index.html"));
    EXPECT_EQ(response.status_code, 405);
}
