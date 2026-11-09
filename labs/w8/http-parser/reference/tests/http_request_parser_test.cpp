#include "http_request_parser.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppapplab::http;

TEST(HttpRequestParserReferenceTest, ParsesSimpleGet) {
    const std::string wire = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.method, "GET");
    EXPECT_EQ(result.request.target, "/hello");
    EXPECT_EQ(result.request.version, "HTTP/1.1");
    EXPECT_TRUE(result.request.body.empty());
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpRequestParserReferenceTest, ParsesPostBodyUsingContentLength) {
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "hello");
}

TEST(HttpRequestParserReferenceTest, NeedsMoreDataForPartialHeader) {
    const ParseResult result =
        HttpRequestParser{}.parse("GET /hello HTTP/1.1\r\nHost: local");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
}

TEST(HttpRequestParserReferenceTest, NeedsMoreDataForPartialBody) {
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
    EXPECT_EQ(result.consumed, 0U);
}

TEST(HttpRequestParserReferenceTest, ReportsConsumedBytesForFirstPipelinedRequest) {
    const std::string first = "GET /a HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const std::string second = "GET /b HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(first + second);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.target, "/a");
    EXPECT_EQ(result.consumed, first.size());
}

TEST(HttpRequestParserReferenceTest, RejectsHttp11WithoutHost) {
    const ParseResult result = HttpRequestParser{}.parse("GET / HTTP/1.1\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "missing Host header");
}

TEST(HttpRequestParserReferenceTest, AcceptsHttp10WithoutHost) {
    const std::string wire = "GET / HTTP/1.0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpRequestParserReferenceTest, RejectsInvalidContentLength) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid Content-Length");
}

TEST(HttpRequestParserReferenceTest, RejectsDuplicateContentLength) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n"
        "content-length: 5\r\n\r\nhello";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
}

TEST(HttpRequestParserReferenceTest, RejectsTransferEncoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "Transfer-Encoding is not supported");
}
