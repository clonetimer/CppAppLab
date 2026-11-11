#include "http_request_parser.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppapplab::http;

namespace {

const std::string kRequestPrefix =
    "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n";

[[nodiscard]] ParseResult parse_chunked(std::string_view body_wire) {
    return HttpRequestParser{}.parse(kRequestPrefix + std::string(body_wire));
}

}  // namespace

TEST(HttpChunkedParserAcceptanceTest, ParsesSingleChunk) {
    const ParseResult result = parse_chunked("5\r\nhello\r\n0\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "hello");
    EXPECT_EQ(result.consumed, kRequestPrefix.size() + 15U);
}

TEST(HttpChunkedParserAcceptanceTest, ParsesMultipleChunksInOrder) {
    const ParseResult result = parse_chunked("3\r\none\r\n3\r\ntwo\r\n0\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "onetwo");
}

TEST(HttpChunkedParserAcceptanceTest, AcceptsCaseInsensitiveChunkedCoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: ChUnKeD\r\n\r\n"
        "1\r\nx\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "x");
}

TEST(HttpChunkedParserAcceptanceTest, AcceptsOptionalWhitespaceAroundCoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: \tchunked \t\r\n\r\n"
        "1\r\nx\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "x");
}

TEST(HttpChunkedParserAcceptanceTest, AcceptsOpaqueVisibleChunkExtensions) {
    const ParseResult result = parse_chunked("3;name=value;flag\r\nabc\r\n0\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "abc");
}

TEST(HttpChunkedParserAcceptanceTest, ValidatesAndConsumesTrailerFields) {
    const ParseResult result = parse_chunked("1\r\nx\r\n0\r\nX-Trace: abc\r\nX-Count: 2\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "x");
    EXPECT_EQ(result.request.headers.count("x-trace"), 0U);
    EXPECT_EQ(result.consumed,
              kRequestPrefix.size() +
                  std::string("1\r\nx\r\n0\r\nX-Trace: abc\r\nX-Count: 2\r\n\r\n").size());
}

TEST(HttpChunkedParserAcceptanceTest, ParsesEmptyChunkedBody) {
    const ParseResult result = parse_chunked("0\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_TRUE(result.request.body.empty());
}

TEST(HttpChunkedParserAcceptanceTest, PreservesBinaryChunkBytes) {
    std::string body("\0\x01\x7f\xff", 4U);
    const std::string wire = kRequestPrefix + "4\r\n" + body + "\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, body);
}

TEST(HttpChunkedParserAcceptanceTest, PreservesCrlfBytesInsideChunkData) {
    const ParseResult result = parse_chunked("5\r\na\r\nb!\r\n0\r\n\r\n");
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "a\r\nb!");
}

TEST(HttpChunkedParserAcceptanceTest, ReportsConsumedBytesBeforePipelinedRequest) {
    const std::string first_body = "3\r\nabc\r\n0\r\n\r\n";
    const std::string first = kRequestPrefix + first_body;
    const std::string second = "GET /next HTTP/1.1\r\nHost: localhost\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(first + second);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "abc");
    EXPECT_EQ(result.consumed, first.size());
}

TEST(HttpChunkedParserAcceptanceTest, NeedsMoreDataForPartialChunkSizeLine) {
    const ParseResult result = parse_chunked("3;name=value");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
    EXPECT_EQ(result.consumed, 0U);
}

TEST(HttpChunkedParserAcceptanceTest, NeedsMoreDataForPartialChunkData) {
    const ParseResult result = parse_chunked("5\r\nhel");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
    EXPECT_EQ(result.consumed, 0U);
}

TEST(HttpChunkedParserAcceptanceTest, NeedsMoreDataForPartialChunkTerminator) {
    const ParseResult result = parse_chunked("3\r\nabc\r");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
}

TEST(HttpChunkedParserAcceptanceTest, NeedsMoreDataForPartialTrailerLine) {
    const ParseResult result = parse_chunked("0\r\nX-Trace: value\r");
    EXPECT_EQ(result.status, ParseStatus::NeedMoreData);
}

TEST(HttpChunkedParserAcceptanceTest, RejectsNonHexadecimalChunkSize) {
    const ParseResult result = parse_chunked("g\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP chunk size");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsNegativeChunkSize) {
    const ParseResult result = parse_chunked("-1\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP chunk size");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsChunkSizeOverflow) {
    const ParseResult result = parse_chunked("FFFFFFFFFFFFFFFFFFFFFFFF\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP chunk size");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsChunkedBodyLargerThanOneMiB) {
    const ParseResult result = parse_chunked("100001\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP body too large");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsTransferEncodingOnHttp10) {
    const std::string wire = "POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "Transfer-Encoding requires HTTP/1.1");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsContentLengthCombinedWithTransferEncoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n"
        "Transfer-Encoding: chunked\r\n\r\n1\r\nx\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "Content-Length and Transfer-Encoding cannot be combined");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsUnsupportedTransferCoding) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "unsupported Transfer-Encoding");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsTransferCodingChains) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\n"
        "Transfer-Encoding: gzip, chunked\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "unsupported Transfer-Encoding");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsMalformedTrailerField) {
    const ParseResult result = parse_chunked("0\r\nMissingColon\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP trailer field");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsContentLengthTrailer) {
    const ParseResult result = parse_chunked("0\r\nContent-Length: 0\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "forbidden HTTP trailer field");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsTransferEncodingTrailer) {
    const ParseResult result = parse_chunked("0\r\nTransfer-Encoding: chunked\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "forbidden HTTP trailer field");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsDuplicateTrailerFieldsCaseInsensitively) {
    const ParseResult result = parse_chunked("0\r\nX-Trace: first\r\nx-trace: second\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "duplicate HTTP trailer field");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsControlBytesInTrailerValues) {
    std::string wire = kRequestPrefix + "0\r\nX-Trace: safe";
    wire.push_back('\x01');
    wire += "bad\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP trailer field");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsOversizedChunkSizeLine) {
    const ParseResult result = parse_chunked(std::string(1025U, 'a') + "\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP chunk-size line too large");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsOversizedTrailerSection) {
    const ParseResult result = parse_chunked("0\r\n" + std::string(16U * 1024U + 1U, 'a'));
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP trailer section too large");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsChunkDataWithoutCrlfTerminator) {
    const ParseResult result = parse_chunked("1\r\nxXY");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "HTTP chunk data is not followed by CRLF");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsEmptyChunkExtension) {
    const ParseResult result = parse_chunked("1;\r\nx\r\n0\r\n\r\n");
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP chunk extension");
}

TEST(HttpChunkedParserAcceptanceTest, KeepsContentLengthFramingWorking) {
    const std::string wire =
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_EQ(result.request.body, "hello");
    EXPECT_EQ(result.consumed, wire.size());
}

TEST(HttpChunkedParserAcceptanceTest, MissingFramingMeansEmptyBody) {
    const std::string wire = "POST /upload HTTP/1.1\r\nHost: localhost\r\n\r\ntrailing";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    ASSERT_EQ(result.status, ParseStatus::Complete);
    EXPECT_TRUE(result.request.body.empty());
    EXPECT_EQ(result.consumed, wire.find("trailing"));
}

TEST(HttpChunkedParserAcceptanceTest, RejectsDuplicateTransferEncodingHeaders) {
    const std::string wire =
        "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "transfer-encoding: chunked\r\n\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid or duplicate header");
}

TEST(HttpChunkedParserAcceptanceTest, RejectsInvalidChunkExtensionControlBytes) {
    std::string wire = kRequestPrefix + "1;name=";
    wire.push_back('\x01');
    wire += "\r\nx\r\n0\r\n\r\n";
    const ParseResult result = HttpRequestParser{}.parse(wire);
    EXPECT_EQ(result.status, ParseStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP chunk extension");
}
