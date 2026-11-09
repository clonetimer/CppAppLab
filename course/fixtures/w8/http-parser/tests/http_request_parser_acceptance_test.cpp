#include "http_request_parser.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppapplab::http;

TEST(HttpRequestParserAcceptanceTest, ParsesSimpleGet) {
    const std::string wire = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.method, "GET");
    EXPECT_EQ(result.request.target, "/hello");
    EXPECT_EQ(result.request.version, "HTTP/1.1");
    EXPECT_TRUE(result.request.body.empty());
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpRequestParserAcceptanceTest, ParsesPostBodyUsingContentLength) {
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "hello");
}

TEST(HttpRequestParserAcceptanceTest, NeedsMoreDataForPartialHeader) {
    const ParseResult result = HttpRequestParser{}.parse("GET /hello HTTP/1.1\r\nHost: local");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
}

TEST(HttpRequestParserAcceptanceTest, NeedsMoreDataForPartialBody) {
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
    EXPECT_EQ(result.consumed, 0U);
}

TEST(HttpRequestParserAcceptanceTest, ReportsConsumedBytesForFirstPipelinedRequest) {
    const std::string first = "GET /a HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const std::string second = "GET /b HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(first + second);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.target, "/a");
    EXPECT_EQ(result.consumed, first.size());
}

TEST(HttpRequestParserAcceptanceTest, RejectsHttp11WithoutHost) {
    const ParseResult result = HttpRequestParser{}.parse("GET / HTTP/1.1\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "missing Host header");
}

TEST(HttpRequestParserAcceptanceTest, AcceptsHttp10WithoutHost) {
    const std::string wire = "GET / HTTP/1.0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpRequestParserAcceptanceTest, RejectsInvalidContentLength) {
    const std::string wire = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsNegativeContentLength) {
    const std::string wire = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsContentLengthSuffix) {
    const std::string wire = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5abc\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsDuplicateContentLength) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n"
        "content-length: 5\r\n\r\nhello";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsDuplicateHeaderNamesCaseInsensitively) {
    const std::string wire = "GET / HTTP/1.1\r\nHost: localhost\r\nhost: other\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsTransferEncoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "Transfer-Encoding is not supported");
}

TEST(HttpRequestParserAcceptanceTest, RejectsHeaderLargerThanSixteenKiB) {
    const std::string wire(16U * 1024U + 1U, 'A');
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP header too large");
}

TEST(HttpRequestParserAcceptanceTest, RejectsBodyLargerThanOneMiB) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1048577\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP body too large");
}

TEST(HttpRequestParserAcceptanceTest, RejectsHeaderWithoutColon) {
    const std::string wire = "GET / HTTP/1.1\r\nHost: localhost\r\nBadHeader\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsEmptyHeaderName) {
    const std::string wire = "GET / HTTP/1.1\r\n: value\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsUnsupportedHttpVersion) {
    const std::string wire = "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsExtraRequestLineToken) {
    const std::string wire = "GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsWhitespaceBeforeHeaderColon) {
    const std::string wire = "GET / HTTP/1.1\r\nHost : localhost\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsInvalidHeaderFieldNameCharacters) {
    const std::string wire = "GET / HTTP/1.1\r\nHost: localhost\r\nBad@Name: value\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, RejectsControlBytesInHeaderValues) {
    std::string wire = "GET / HTTP/1.1\r\nHost: localhost\r\nX-Test: safe";
    wire.push_back('\x01');
    wire += "value\r\n\r\n";
    EXPECT_EQ(HttpRequestParser{}.parse(wire).status, ParseStatus::Error);
}

TEST(HttpRequestParserAcceptanceTest, LowercasesHeaderNames) {
    const std::string wire = "GET / HTTP/1.1\r\nHOST: localhost\r\nX-Trace: yes\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_NE(result.request.headers.find("host"), result.request.headers.end());
    EXPECT_NE(result.request.headers.find("x-trace"), result.request.headers.end());
}

TEST(HttpRequestParserAcceptanceTest, BodyMayContainHeaderTerminatorBytes) {
    const std::string body = "a\r\n\r\nb";
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\n\r\n" + body;
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, body);
}

TEST(HttpRequestParserAcceptanceTest, ContentLengthCountsUtf8Bytes) {
    const std::string body = "你好";
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\n\r\n" + body;
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, body);
}

TEST(HttpRequestParserAcceptanceTest, MissingContentLengthMeansEmptyBody) {
    const std::string header = "POST /echo HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(header + "trailing");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_TRUE(result.request.body.empty());
    EXPECT_EQ(result.consumed, header.size());
}

TEST(HttpRequestParserAcceptanceTest, ZeroContentLengthCompletesAtHeaderBoundary) {
    const std::string wire = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_TRUE(result.request.body.empty());
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpRequestParserAcceptanceTest, PartialBodyDoesNotConsumeBytes) {
    const std::string wire =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 9\r\n\r\nshort";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
    EXPECT_EQ(result.consumed, 0U);
}
