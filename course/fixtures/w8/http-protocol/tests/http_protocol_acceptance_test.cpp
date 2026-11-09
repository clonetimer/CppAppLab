#include "http_message.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace cppapplab::http;

TEST(HttpProtocolAcceptanceTest, ParsesGetRequestLine) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("GET / HTTP/1.1", request));
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.target, "/");
    EXPECT_EQ(request.version, "HTTP/1.1");
}
TEST(HttpProtocolAcceptanceTest, ParsesPostRequestLine) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("POST /echo HTTP/1.1", request));
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.target, "/echo");
    EXPECT_EQ(request.version, "HTTP/1.1");
}
TEST(HttpProtocolAcceptanceTest, ParsesDeleteRequestLine) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("DELETE /users/1 HTTP/1.1", request));
    EXPECT_EQ(request.method, "DELETE");
    EXPECT_EQ(request.target, "/users/1");
}
TEST(HttpProtocolAcceptanceTest, RejectsEmptyRequestLine) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("", request));
}
TEST(HttpProtocolAcceptanceTest, RejectsMethodWithoutTargetAndVersion) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET", request));
}
TEST(HttpProtocolAcceptanceTest, RejectsMissingVersion) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET /hello", request));
}
TEST(HttpProtocolAcceptanceTest, RejectsExtraRequestLineTokens) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET /hello HTTP/1.1 extra", request));
}
TEST(HttpProtocolAcceptanceTest, RejectsEmbeddedLineBreaks) {
    HttpRequest request;
    EXPECT_FALSE(parse_request_line("GET / HTTP/1.1\r\nHost: local", request));
}
TEST(HttpProtocolAcceptanceTest, AcceptsHorizontalTabSeparators) {
    HttpRequest request;
    ASSERT_TRUE(parse_request_line("GET\t/hello\tHTTP/1.1", request));
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.target, "/hello");
}
TEST(HttpProtocolAcceptanceTest, PreservesHeadersAndBodyWhenParsingRequestLine) {
    HttpRequest request;
    request.headers.emplace("Host", "localhost");
    request.body = "payload";
    ASSERT_TRUE(parse_request_line("POST /items HTTP/1.1", request));
    EXPECT_EQ(request.headers.at("Host"), "localhost");
    EXPECT_EQ(request.body, "payload");
}
TEST(HttpProtocolAcceptanceTest, SerializesDefaultStatusLineAndEmptyBody) {
    const std::string wire = serialize_response(HttpResponse{});
    EXPECT_EQ(wire, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
}
TEST(HttpProtocolAcceptanceTest, SerializesNotFoundStatus) {
    HttpResponse response;
    response.status_code = 404;
    response.reason_phrase = "Not Found";
    EXPECT_NE(serialize_response(response).find("HTTP/1.1 404 Not Found\r\n"), std::string::npos);
}
TEST(HttpProtocolAcceptanceTest, SerializesContentTypeHeader) {
    HttpResponse response;
    response.headers.emplace("Content-Type", "text/plain");
    EXPECT_NE(serialize_response(response).find("Content-Type: text/plain\r\n"), std::string::npos);
}
TEST(HttpProtocolAcceptanceTest, ContentLengthCountsUtf8Bytes) {
    HttpResponse response;
    response.body = "你好";
    EXPECT_NE(serialize_response(response).find("Content-Length: 6\r\n"), std::string::npos);
}
TEST(HttpProtocolAcceptanceTest, ForcesConnectionCloseInsteadOfKeepAlive) {
    HttpResponse response;
    response.headers.emplace("Connection", "keep-alive");
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("keep-alive"), std::string::npos);
}
TEST(HttpProtocolAcceptanceTest, ReplacesCallerSuppliedContentLength) {
    HttpResponse response;
    response.headers.emplace("Content-Length", "999");
    response.body = "abc";
    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Content-Length: 3\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("999"), std::string::npos);
}
TEST(HttpProtocolAcceptanceTest, SerializesHeadersInStableOrder) {
    HttpResponse response;
    response.headers.emplace("X-Zeta", "z");
    response.headers.emplace("Content-Type", "text/plain");
    response.body = "ok";
    const std::string wire = serialize_response(response);
    const auto content_type = wire.find("Content-Type: text/plain\r\n");
    const auto x_zeta = wire.find("X-Zeta: z\r\n");
    ASSERT_NE(content_type, std::string::npos);
    ASSERT_NE(x_zeta, std::string::npos);
    EXPECT_LT(content_type, x_zeta);
}
TEST(HttpProtocolAcceptanceTest, RejectsStatusOutsideHttpRange) {
    HttpResponse response;
    response.status_code = 99;
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
TEST(HttpProtocolAcceptanceTest, RejectsInvalidHeaderName) {
    HttpResponse response;
    response.headers.emplace("Bad Name", "value");
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
TEST(HttpProtocolAcceptanceTest, RejectsCarriageReturnInHeaderValue) {
    HttpResponse response;
    response.headers.emplace("X-Test", "safe\r\nInjected: yes");
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
TEST(HttpProtocolAcceptanceTest, RejectsCaseInsensitiveDuplicateHeaders) {
    HttpResponse response;
    response.headers.emplace("X-Mode", "one");
    response.headers.emplace("x-mode", "two");
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
TEST(HttpProtocolAcceptanceTest, RejectsReasonPhraseLineBreaks) {
    HttpResponse response;
    response.reason_phrase = "OK\r\nInjected: yes";
    EXPECT_THROW(serialize_response(response), std::invalid_argument);
}
