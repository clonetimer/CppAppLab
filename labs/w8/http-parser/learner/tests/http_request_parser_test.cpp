#include "http_request_parser.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppapplab::http;

TEST(HttpRequestParserLearnerTest, ParsesSimpleGet) {
    FAIL() << "TODO(W8-PARSER-TEST-1): parse a complete GET request.";
}
TEST(HttpRequestParserLearnerTest, ParsesPostBodyUsingContentLength) {
    FAIL() << "TODO(W8-PARSER-TEST-2): parse a POST body using Content-Length.";
}
TEST(HttpRequestParserLearnerTest, NeedsMoreDataForPartialHeader) {
    FAIL() << "TODO(W8-PARSER-TEST-3): return NeedMoreData before CRLFCRLF arrives.";
}
TEST(HttpRequestParserLearnerTest, NeedsMoreDataForPartialBody) {
    FAIL() << "TODO(W8-PARSER-TEST-4): return NeedMoreData until the declared body is complete.";
}
TEST(HttpRequestParserLearnerTest, RejectsHttp11WithoutHost) {
    FAIL() << "TODO(W8-PARSER-TEST-5): require Host for HTTP/1.1.";
}
TEST(HttpRequestParserLearnerTest, RejectsInvalidContentLength) {
    FAIL() << "TODO(W8-PARSER-TEST-6): reject non-decimal Content-Length values.";
}
TEST(HttpRequestParserLearnerTest, RejectsDuplicateHeaders) {
    FAIL() << "TODO(W8-PARSER-TEST-7): reject duplicate headers case-insensitively.";
}
TEST(HttpRequestParserLearnerTest, RejectsTransferEncoding) {
    FAIL() << "TODO(W8-PARSER-TEST-8): reject unsupported Transfer-Encoding.";
}
TEST(HttpRequestParserLearnerTest, EnforcesHeaderAndBodyLimits) {
    FAIL() << "TODO(W8-PARSER-TEST-9): enforce the 16 KiB header and 1 MiB body limits.";
}
TEST(HttpRequestParserLearnerTest, ReportsConsumedBytesForPipelinedRequests) {
    FAIL() << "TODO(W8-PARSER-TEST-10): report exactly the first request's byte count.";
}
