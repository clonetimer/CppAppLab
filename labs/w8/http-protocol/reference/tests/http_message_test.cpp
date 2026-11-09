#include "http_message.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace cppapplab::http;

TEST(HttpProtocolReferenceTest, ParsesGetRequestLine) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("GET /hello HTTP/1.1", request));
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.target, "/hello");
    EXPECT_EQ(request.version, "HTTP/1.1");
}

TEST(HttpProtocolReferenceTest, ParsesPostRequestLine) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("POST /echo HTTP/1.1", request));
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.target, "/echo");
    EXPECT_EQ(request.version, "HTTP/1.1");
}

TEST(HttpProtocolReferenceTest, RejectsIncompleteRequestLine) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET /hello", request));
}

TEST(HttpProtocolReferenceTest, RejectsExtraRequestLineTokens) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET /hello HTTP/1.1 extra", request));
}

TEST(HttpProtocolReferenceTest, PreservesExistingHeadersAndBody) {
    HttpRequest request;
    request.headers.emplace("Host", "localhost");
    request.body = "payload";
    ASSERT_TRUE(parse_request_line("POST /items HTTP/1.1", request));
    EXPECT_EQ(request.headers.at("Host"), "localhost");
    EXPECT_EQ(request.body, "payload");
}

TEST(HttpProtocolReferenceTest, SerializesDefaultStatusLine) {
    const HttpResponse response;
    const std::string wire = serialize_response(response);
    EXPECT_EQ(wire.rfind("HTTP/1.1 200 OK\r\n", 0U), 0U);
}

TEST(HttpProtocolReferenceTest, ComputesContentLengthFromBodyBytes) {
    HttpResponse response;
    response.body = "你好";
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Content-Length: 6\r\n"), std::string::npos);
}

TEST(HttpProtocolReferenceTest, ForcesConnectionClose) {
    HttpResponse response;
    response.headers.emplace("Connection", "keep-alive");
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("keep-alive"), std::string::npos);
}

TEST(HttpProtocolReferenceTest, PreservesContentType) {
    HttpResponse response;
    response.headers.emplace("Content-Type", "text/plain");
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Content-Type: text/plain\r\n"), std::string::npos);
}

TEST(HttpProtocolReferenceTest, RejectsHeaderInjection) {
    HttpResponse response;
    response.headers.emplace("X-Test", "safe\r\nInjected: yes");
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
