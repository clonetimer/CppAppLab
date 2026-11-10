#include "router.hpp"
#include "api_handlers.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace cppapplab::http;

namespace {

HttpRequest get(const std::string& target) {
    HttpRequest request;
    request.method = "GET";
    request.target = target;
    return request;
}

}  // namespace

TEST(RouterReferenceTest, RegistersAndInvokesHandler) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    EXPECT_EQ(router.handle(get("/hello")).body, "Hello from C++ HTTP Server!\n");
}

TEST(RouterReferenceTest, DistinguishesMethods) {
    const auto router = make_default_router();
    EXPECT_EQ(router.handle(get("/echo")).status_code, 405);
}

TEST(RouterReferenceTest, IgnoresQueryForMatching) {
    EXPECT_EQ(make_default_router().handle(get("/hello?x=1")).status_code, 200);
}

TEST(RouterReferenceTest, ReplacesSameRoute) {
    Router router;
    router.add_route("GET", "/hello", hello_handler);
    router.add_route("GET", "/hello", health_handler);
    EXPECT_EQ(router.handle(get("/hello")).body, R"({"status":"ok"})");
}

TEST(RouterReferenceTest, EchoesBinaryBody) {
    auto request = get("/echo");
    request.method = "POST";
    request.body = std::string("A\0B", 3U);
    EXPECT_EQ(make_default_router().handle(request).body, request.body);
}

TEST(RouterReferenceTest, HealthUsesJsonMimeType) {
    const auto response = health_handler({});
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST(RouterReferenceTest, RegistersDefaultEndpoints) {
    const auto router = make_default_router();
    EXPECT_TRUE(router.has_route("GET", "/hello"));
    EXPECT_TRUE(router.has_route("POST", "/echo"));
    EXPECT_TRUE(router.has_route("GET", "/health"));
}

TEST(RouterReferenceTest, RejectsEmptyCallback) {
    Router router;
    EXPECT_THROW(router.add_route("GET", "/hello", {}), std::invalid_argument);
}

TEST(RouterReferenceTest, UnknownDynamicPathReturns404) {
    EXPECT_EQ(Router{}.handle(get("/missing")).status_code, 404);
}

TEST(RouterReferenceTest, MethodErrorIncludesAllow) {
    const auto response = make_default_router().handle(get("/echo"));
    EXPECT_EQ(response.headers.at("Allow"), "POST");
}
