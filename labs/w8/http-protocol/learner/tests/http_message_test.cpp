#include "http_message.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace cppapplab::http;

TEST(HttpProtocolLearnerTest, ParsesGetRequestLine) {
    FAIL() << "TODO(W8-HTTP-TEST-1): parse GET / HTTP/1.1 into its three fields.";
}
TEST(HttpProtocolLearnerTest, ParsesPostRequestLine) {
    FAIL() << "TODO(W8-HTTP-TEST-2): parse POST /echo HTTP/1.1.";
}
TEST(HttpProtocolLearnerTest, RejectsMissingRequestLineFields) {
    FAIL() << "TODO(W8-HTTP-TEST-3): reject an incomplete request line.";
}
TEST(HttpProtocolLearnerTest, RejectsExtraRequestLineTokens) {
    FAIL() << "TODO(W8-HTTP-TEST-4): reject a request line with extra tokens.";
}
TEST(HttpProtocolLearnerTest, PreservesExistingRequestHeadersAndBody) {
    FAIL() << "TODO(W8-HTTP-TEST-5): parse the request line without destroying other fields.";
}
TEST(HttpProtocolLearnerTest, SerializesStatusLine) {
    FAIL() << "TODO(W8-HTTP-TEST-6): serialize HTTP/1.1 status code and reason phrase.";
}
TEST(HttpProtocolLearnerTest, ComputesContentLengthFromBodyBytes) {
    FAIL() << "TODO(W8-HTTP-TEST-7): verify Content-Length matches the body byte count.";
}
TEST(HttpProtocolLearnerTest, AddsConnectionCloseForFirstVersion) {
    FAIL() << "TODO(W8-HTTP-TEST-8): verify Connection: close is emitted.";
}
TEST(HttpProtocolLearnerTest, PreservesContentTypeHeader) {
    FAIL() << "TODO(W8-HTTP-TEST-9): verify caller-supplied Content-Type is serialized.";
}
TEST(HttpProtocolLearnerTest, RejectsHeaderInjection) {
    FAIL() << "TODO(W8-HTTP-TEST-10): reject CR/LF in header values.";
}
