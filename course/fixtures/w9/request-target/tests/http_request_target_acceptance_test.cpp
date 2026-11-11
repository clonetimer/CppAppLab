#include "http_request_target.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppapplab::http;

TEST(HttpRequestTargetAcceptanceTest, ParsesOriginFormPath) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/42");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_EQ(result.target.path, "/items/42");
    EXPECT_TRUE(result.target.query.empty());
    EXPECT_FALSE(result.target.asterisk_form);
}

TEST(HttpRequestTargetAcceptanceTest, ParsesQueryParameters) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/search?tag=cpp&sort=recent");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 2U);
    EXPECT_EQ(result.target.query[0].name, "tag");
    EXPECT_EQ(result.target.query[0].value, "cpp");
    EXPECT_EQ(result.target.query[1].name, "sort");
    EXPECT_EQ(result.target.query[1].value, "recent");
}

TEST(HttpRequestTargetAcceptanceTest, PreservesRepeatedQueryKeysInOrder) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/search?tag=cpp&tag=linux&tag=networking");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 3U);
    EXPECT_EQ(result.target.query[0].name, "tag");
    EXPECT_EQ(result.target.query[0].value, "cpp");
    EXPECT_EQ(result.target.query[1].value, "linux");
    EXPECT_EQ(result.target.query[2].value, "networking");
}

TEST(HttpRequestTargetAcceptanceTest, AllowsEmptyQueryValues) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?empty=");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].value, "");
}

TEST(HttpRequestTargetAcceptanceTest, TreatsMissingEqualsAsEmptyValue) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?verbose");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].name, "verbose");
    EXPECT_EQ(result.target.query[0].value, "");
}

TEST(HttpRequestTargetAcceptanceTest, PreservesPlusAsLiteralPlus) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?q=two+words");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].value, "two+words");
}

TEST(HttpRequestTargetAcceptanceTest, DecodesPercentEncodedPathBytes) {
    const RequestTargetParseResult result = parse_request_target("GET", "/files/a%20b.txt");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_EQ(result.target.path, "/files/a b.txt");
}

TEST(HttpRequestTargetAcceptanceTest, DecodesPercentEncodedQueryComponents) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/search?na%6De=two%20words");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].name, "name");
    EXPECT_EQ(result.target.query[0].value, "two words");
}

TEST(HttpRequestTargetAcceptanceTest, DecodesPercentEncodingExactlyOnce) {
    const RequestTargetParseResult result = parse_request_target("GET", "/files/%252e%252e");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_EQ(result.target.path, "/files/%2e%2e");
}

TEST(HttpRequestTargetAcceptanceTest, PreservesTrailingSlash) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_EQ(result.target.path, "/items/");
}

TEST(HttpRequestTargetAcceptanceTest, AcceptsRootPath) {
    const RequestTargetParseResult result = parse_request_target("GET", "/");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_EQ(result.target.path, "/");
}

TEST(HttpRequestTargetAcceptanceTest, AcceptsAsteriskFormForOptions) {
    const RequestTargetParseResult result = parse_request_target("OPTIONS", "*");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    EXPECT_TRUE(result.target.asterisk_form);
    EXPECT_TRUE(result.target.path.empty());
}

TEST(HttpRequestTargetAcceptanceTest, RejectsAsteriskFormForGet) {
    const RequestTargetParseResult result = parse_request_target("GET", "*");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "asterisk-form is only valid for OPTIONS");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsAbsoluteForm) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "http://example.test/items");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "unsupported request-target form");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsAuthorityForm) {
    const RequestTargetParseResult result = parse_request_target("CONNECT", "example.test:443");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "unsupported request-target form");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEmptyTarget) {
    const RequestTargetParseResult result = parse_request_target("GET", "");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "empty request target");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsFragment) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items#details");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target contains an invalid raw byte or fragment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsRawSpace) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/a b");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target contains an invalid raw byte or fragment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsRawControlByte) {
    std::string target = "/items/";
    target.push_back('\x01');
    const RequestTargetParseResult result = parse_request_target("GET", target);
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target contains an invalid raw byte or fragment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsMalformedPercentEscape) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/%GG");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "invalid percent-encoding");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsIncompletePercentEscape) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/%A");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "invalid percent-encoding");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEncodedNul) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/%00");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "decoded request target contains a control byte");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEncodedControlByte) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/%0A");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "decoded request target contains a control byte");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEncodedSlashInPath) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/a%2Fb");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "encoded path separator is not allowed");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEncodedBackslashInPath) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/a%5Cb");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "encoded path separator is not allowed");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsRawBackslash) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/a\\b");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target contains an invalid raw byte or fragment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsDotSegment) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/../secret");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request path contains dot segment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEncodedDotSegment) {
    const RequestTargetParseResult result = parse_request_target("GET", "/items/%2e%2e/secret");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request path contains dot segment");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEmptyQueryName) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?=value");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "query parameter name is empty");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsEmptyQueryParameterSegment) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?a=1&&b=2");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "empty query parameter");
}

TEST(HttpRequestTargetAcceptanceTest, EnforcesQueryParameterCountLimit) {
    std::string target = "/search?";
    for (std::size_t index = 0U; index < 129U; ++index) {
        if (index != 0U) {
            target.push_back('&');
        }
        target += "k=v";
    }
    const RequestTargetParseResult result = parse_request_target("GET", target);
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "query parameter limit exceeded");
}

TEST(HttpRequestTargetAcceptanceTest, EnforcesTotalTargetSizeLimit) {
    const RequestTargetParseResult result = parse_request_target("GET", std::string(8193U, 'a'));
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target too large");
}

TEST(HttpRequestTargetAcceptanceTest, EnforcesPathSizeLimit) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/" + std::string(4096U, 'a'));
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request path too large");
}

TEST(HttpRequestTargetAcceptanceTest, EnforcesQuerySizeLimit) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/?" + std::string(4097U, 'a'));
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "query string too large");
}

TEST(HttpRequestTargetAcceptanceTest, EnforcesQueryComponentSizeLimit) {
    const RequestTargetParseResult result =
        parse_request_target("GET", "/?key=" + std::string(1025U, 'a'));
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request-target component too large");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsNonAsciiRawTargetBytes) {
    std::string target = "/items/";
    target.push_back(static_cast<char>(0xc3));
    const RequestTargetParseResult result = parse_request_target("GET", target);
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "request target contains an invalid raw byte or fragment");
}

TEST(HttpRequestTargetAcceptanceTest, AllowsEncodedSlashInQueryValue) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?q=a%2Fb");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].value, "a/b");
}

TEST(HttpRequestTargetAcceptanceTest, PreservesEqualsInsideQueryValue) {
    const RequestTargetParseResult result = parse_request_target("GET", "/search?token=a=b=c");
    ASSERT_EQ(result.status, RequestTargetStatus::Complete);
    ASSERT_EQ(result.target.query.size(), 1U);
    EXPECT_EQ(result.target.query[0].value, "a=b=c");
}

TEST(HttpRequestTargetAcceptanceTest, RejectsInvalidMethod) {
    const RequestTargetParseResult result = parse_request_target("GET BAD", "/items");
    EXPECT_EQ(result.status, RequestTargetStatus::Error);
    EXPECT_EQ(result.error, "invalid HTTP method");
}
