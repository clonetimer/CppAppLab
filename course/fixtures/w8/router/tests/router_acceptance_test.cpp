#include "router.hpp"
#include "api_handlers.hpp"
#include "dispatcher.hpp"
#include "http_request_parser.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace cppapplab::http;

namespace {

HttpRequest request(std::string method, std::string target, std::string body = {}) {
    HttpRequest result;
    result.method = std::move(method);
    result.target = std::move(target);
    result.body = std::move(body);
    return result;
}

HttpResponse reply(std::string body) {
    HttpResponse result;
    result.body = std::move(body);
    return result;
}

class RouterAcceptanceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("router-test-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    void write_file(const std::string& path, const std::string& bytes) {
        std::ofstream output(root_ / path, std::ios::binary);
        ASSERT_TRUE(output.good());
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(output.good());
    }
    HttpResponse dispatch(const HttpRequest& incoming, const Router& router) const {
        return dispatch_request(incoming, router, StaticFileHandler(root_));
    }
    std::filesystem::path root_;
};

}  // namespace

TEST_F(RouterAcceptanceTest, EmptyRouterHasNoRoute) {
    const Router router;
    EXPECT_FALSE(router.has_route("GET", "/hello"));
    EXPECT_FALSE(router.has_path("/hello"));
}

TEST_F(RouterAcceptanceTest, RoutesMatchingRequest) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    EXPECT_TRUE(router.has_route("GET", "/hello"));
    EXPECT_EQ(router.handle(request("GET", "/hello")).body, "Hello from C++ HTTP Server!\n");
}

TEST_F(RouterAcceptanceTest, MethodIsPartOfTheRoute) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    EXPECT_FALSE(router.has_route("POST", "/hello"));
    EXPECT_TRUE(router.has_path("/hello"));
}

TEST_F(RouterAcceptanceTest, DifferentMethodsHaveIndependentHandlers) {
    Router router;
    router.add_route("GET", "/same", hello_handler);
    router.add_route("POST", "/same", echo_handler);
    EXPECT_EQ(router.handle(request("POST", "/same", "posted")).body, "posted");
    EXPECT_EQ(router.handle(request("GET", "/same")).body, "Hello from C++ HTTP Server!\n");
}

TEST_F(RouterAcceptanceTest, MethodMatchingIsCaseSensitive) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    EXPECT_FALSE(router.has_route("get", "/hello"));
    EXPECT_EQ(router.handle(request("get", "/hello")).status_code, 405);
}

TEST_F(RouterAcceptanceTest, PathMatchingIsExactRatherThanPrefixBased) {
    const auto router = make_default_router();
    EXPECT_EQ(router.handle(request("GET", "/hello/child")).status_code, 404);
    EXPECT_EQ(router.handle(request("GET", "/Hello")).status_code, 404);
}

TEST_F(RouterAcceptanceTest, IgnoresQueryForMatching) {
    const auto router = make_default_router();
    EXPECT_EQ(router.handle(request("GET", "/hello?name=Alice")).status_code, 200);
}

TEST_F(RouterAcceptanceTest, CallbackReceivesUnmodifiedRequest) {
    Router router;
    router.add_route("POST", "/inspect", [](const HttpRequest& incoming) {
        return reply(incoming.target + "|" + incoming.headers.at("x-test") + incoming.body);
    });
    auto incoming = request("POST", "/inspect?x=1", "payload");
    incoming.headers.emplace("x-test", "header");
    EXPECT_EQ(router.handle(incoming).body, "/inspect?x=1|headerpayload");
    EXPECT_EQ(incoming.body, "payload");
}

TEST_F(RouterAcceptanceTest, DuplicateRegistrationReplacesHandler) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    router.add_route("GET", "/hello", [](const HttpRequest&) { return reply("replacement"); });
    EXPECT_EQ(router.handle(request("GET", "/hello")).body, "replacement");
}

TEST_F(RouterAcceptanceTest, EmptyHandlerRejectionPreservesExistingRoute) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    EXPECT_THROW(router.add_route("GET", "/hello", {}), std::invalid_argument);
    EXPECT_EQ(router.handle(request("GET", "/hello")).status_code, 200);
}

TEST_F(RouterAcceptanceTest, RejectsInvalidRegistrationWithoutAddingAPath) {
    Router router;
    const std::vector<std::string> methods{"", "G ET", "GET\r\n", "G@T"};
    for (const auto& method : methods) {
        EXPECT_THROW(router.add_route(method, "/x", hello_handler), std::invalid_argument);
    }
    const std::vector<std::string> paths{"", "relative", "/x?q=1", "/x#f", "/bad path"};
    for (const auto& path : paths) {
        EXPECT_THROW(router.add_route("GET", path, hello_handler), std::invalid_argument);
    }
    EXPECT_FALSE(router.has_path("/x"));
}

TEST_F(RouterAcceptanceTest, UnknownDynamicPathReturns404) {
    const auto response = Router{}.handle(request("GET", "/missing"));
    EXPECT_EQ(response.status_code, 404);
    EXPECT_EQ(response.reason_phrase, "Not Found");
    EXPECT_EQ(response.body, "404 Not Found\n");
}

TEST_F(RouterAcceptanceTest, KnownPathWrongMethodReturns405WithAllow) {
    const auto router = make_default_router();
    const auto response = router.handle(request("GET", "/echo"));
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.headers.at("Allow"), "POST");
}

TEST_F(RouterAcceptanceTest, AllowListIsSortedAndHasNoDuplicateMethods) {
    Router router;
    router.add_route("POST", "/x", echo_handler);
    router.add_route("GET", "/x", hello_handler);
    router.add_route("GET", "/x", hello_handler);
    const auto response = router.handle(request("DELETE", "/x"));
    EXPECT_EQ(response.headers.at("Allow"), "GET, POST");
}

TEST_F(RouterAcceptanceTest, PreservesCustomHandlerResponse) {
    Router router;
    router.add_route("GET", "/custom", [](const HttpRequest&) {
        auto response = reply("custom failure");
        response.status_code = 503;
        response.reason_phrase = "Service Unavailable";
        response.headers.emplace("X-Source", "handler");
        return response;
    });
    const auto response = router.handle(request("GET", "/custom"));
    EXPECT_EQ(response.status_code, 503);
    EXPECT_EQ(response.headers.at("X-Source"), "handler");
    EXPECT_EQ(response.body, "custom failure");
}

TEST_F(RouterAcceptanceTest, CallbackExceptionsPropagateToApplicationBoundary) {
    Router router;
    router.add_route("GET", "/throws", [](const HttpRequest&) -> HttpResponse {
        throw std::runtime_error("handler failure");
    });
    EXPECT_THROW((void)router.handle(request("GET", "/throws")), std::runtime_error);
}

TEST_F(RouterAcceptanceTest, HelloHandlerReturnsCourseText) {
    const auto response = hello_handler({});
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "Hello from C++ HTTP Server!\n");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain; charset=utf-8");
}

TEST_F(RouterAcceptanceTest, EchoHandlerAcceptsEmptyBody) {
    EXPECT_TRUE(echo_handler({}).body.empty());
}

TEST_F(RouterAcceptanceTest, EchoHandlerPreservesBinaryBody) {
    const std::string bytes("A\0B\nC", 5U);
    EXPECT_EQ(echo_handler(request("POST", "/echo", bytes)).body, bytes);
}

TEST_F(RouterAcceptanceTest, EchoHandlerPreservesUtf8Bytes) {
    EXPECT_EQ(echo_handler(request("POST", "/echo", "你好")).body, "你好");
}

TEST_F(RouterAcceptanceTest, HealthHandlerReturnsJson) {
    const auto response = health_handler({});
    EXPECT_EQ(response.body, R"({"status":"ok"})");
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST_F(RouterAcceptanceTest, RegistersAllThreeCourseEndpoints) {
    const auto router = make_default_router();
    EXPECT_TRUE(router.has_route("GET", "/hello"));
    EXPECT_TRUE(router.has_route("POST", "/echo"));
    EXPECT_TRUE(router.has_route("GET", "/health"));
}

TEST_F(RouterAcceptanceTest, DynamicRouteTakesPriorityOverSameNamedFile) {
    write_file("hello", "must not shadow a dynamic handler");
    const auto response = dispatch(request("GET", "/hello"), make_default_router());
    EXPECT_EQ(response.body, "Hello from C++ HTTP Server!\n");
}

TEST_F(RouterAcceptanceTest, Dynamic404DoesNotFallBackToDisk) {
    write_file("custom", "must not mask the handler response");
    Router router;
    router.add_route("GET", "/custom", [](const HttpRequest&) {
        auto response = reply("dynamic not found");
        response.status_code = 404;
        return response;
    });
    EXPECT_EQ(dispatch(request("GET", "/custom"), router).body, "dynamic not found");
}

TEST_F(RouterAcceptanceTest, DynamicMethodMismatchDoesNotFallBackToDisk) {
    write_file("echo", "must not expose a static collision");
    const auto response = dispatch(request("GET", "/echo?x=1"), make_default_router());
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.headers.at("Allow"), "POST");
}

TEST_F(RouterAcceptanceTest, UnknownDynamicPathFallsBackToStaticIndex) {
    write_file("index.html", "<h1>home</h1>");
    const auto response = dispatch(request("GET", "/"), make_default_router());
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "<h1>home</h1>");
}

TEST_F(RouterAcceptanceTest, StaticFallbackStripsQueryString) {
    write_file("hello.txt", "static bytes");
    const auto response = dispatch(request("GET", "/hello.txt?x=1"), make_default_router());
    EXPECT_EQ(response.body, "static bytes");
}

TEST_F(RouterAcceptanceTest, MissingStaticResourceReturns404) {
    EXPECT_EQ(dispatch(request("GET", "/missing"), make_default_router()).status_code, 404);
}

TEST_F(RouterAcceptanceTest, UnsupportedStaticMethodRetains405) {
    const auto response = dispatch(request("POST", "/index.html"), make_default_router());
    EXPECT_EQ(response.status_code, 405);
    EXPECT_EQ(response.headers.at("Allow"), "GET");
}

TEST_F(RouterAcceptanceTest, StaticTraversalStillRejected) {
    EXPECT_EQ(dispatch(request("GET", "/../secret"), make_default_router()).status_code, 400);
}

TEST_F(RouterAcceptanceTest, ParserDispatcherSerializerPreserveBinaryEcho) {
    const std::string bytes("A\0B\nC", 5U);
    const std::string head = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n";
    const auto parsed = HttpRequestParser{}.parse(head + bytes);
    ASSERT_EQ(parsed.status, ParseStatus::Complete);
    const std::string wire = serialize_response(dispatch(parsed.request, make_default_router()));
    EXPECT_NE(wire.find("Content-Length: 5\r\n"), std::string::npos);
    ASSERT_NE(wire.find("\r\n\r\n"), std::string::npos);
    EXPECT_EQ(wire.substr(wire.find("\r\n\r\n") + 4U), bytes);
}

TEST_F(RouterAcceptanceTest, PartialBodyIsCompletedBeforeDynamicDispatch) {
    const std::string head = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(head + "ab").status, ParseStatus::NeedMoreData);
    const auto parsed = HttpRequestParser{}.parse(head + "abcde");
    ASSERT_EQ(parsed.status, ParseStatus::Complete);
    EXPECT_EQ(dispatch(parsed.request, make_default_router()).body, "abcde");
}
