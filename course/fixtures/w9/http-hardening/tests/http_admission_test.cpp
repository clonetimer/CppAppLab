#include "http_hardening.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace {
using namespace cppapplab::http;

const std::string kGet = "GET / HTTP/1.1\r\nHost: local.test\r\n\r\n";

void expect_hidden(const AdmissionResult& result, AdmissionStatus status) {
    EXPECT_EQ(result.status, status);
    EXPECT_EQ(result.consumed, 0U);
    EXPECT_EQ(result.close_connection, status != AdmissionStatus::NeedMoreData);
    EXPECT_TRUE(result.request.method.empty());
    EXPECT_TRUE(result.request.target.empty());
    EXPECT_TRUE(result.request.version.empty());
    EXPECT_TRUE(result.request.headers.empty());
    EXPECT_TRUE(result.request.body.empty());
}

std::string post(std::size_t size) {
    return "POST / HTTP/1.1\r\nHost: local.test\r\nContent-Length: " +
           std::to_string(size) + "\r\n\r\n";
}

std::string chunked() {
    return "POST / HTTP/1.1\r\nHost: local.test\r\nTransfer-Encoding: chunked\r\n\r\n";
}

std::string header_with_fields(std::size_t fields) {
    std::string message = "GET / HTTP/1.1\r\nHost: local.test\r\n";
    for (std::size_t index = 1; index < fields; ++index) {
        message += "X-Field-" + std::to_string(index) + ": v\r\n";
    }
    return message + "\r\n";
}

TEST(HttpAdmission, EmptyAndPartial) {
    expect_hidden(admit_request(""), AdmissionStatus::NeedMoreData);
    expect_hidden(admit_request(kGet.substr(0, kGet.size() - 1)), AdmissionStatus::NeedMoreData);
}
TEST(HttpAdmission, ValidRequest) {
    const auto result = admit_request(kGet);
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.consumed, kGet.size());
    EXPECT_EQ(result.request.target, "/");
    EXPECT_FALSE(result.close_connection);
}
TEST(HttpAdmission, Http10WithoutHost) {
    const std::string message = "GET / HTTP/1.0\r\n\r\n";
    const auto result = admit_request(message);
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.consumed, message.size());
}
TEST(HttpAdmission, MissingHost) {
    expect_hidden(admit_request("GET / HTTP/1.1\r\n\r\n"), AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, EmptyHost) {
    expect_hidden(admit_request("GET / HTTP/1.1\r\nHost: \t\r\n\r\n"), AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, DuplicateField) {
    expect_hidden(admit_request("GET / HTTP/1.1\r\nHost: local.test\r\nhost: local.test\r\n\r\n"),
                  AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, InvalidField) {
    expect_hidden(admit_request("GET / HTTP/1.1\r\nHost: local.test\r\nInvalid Field: a\r\n\r\n"),
                  AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, AmbiguousFramingRejected) {
    expect_hidden(admit_request("POST / HTTP/1.1\r\nHost: local.test\r\n"
                                "Content-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n"),
                  AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, UnsupportedTransferCoding) {
    expect_hidden(admit_request("POST / HTTP/1.1\r\nHost: local.test\r\n"
                                "Transfer-Encoding: unknown\r\n\r\n"),
                  AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, InvalidLength) {
    expect_hidden(admit_request("POST / HTTP/1.1\r\nHost: local.test\r\nContent-Length: -1\r\n\r\n"),
                  AdmissionStatus::BadRequest);
}
TEST(HttpAdmission, PartialBodyHidden) {
    expect_hidden(admit_request(post(4) + "ab"), AdmissionStatus::NeedMoreData);
}
TEST(HttpAdmission, FixedLengthComplete) {
    const std::string message = post(4) + "data";
    const auto result = admit_request(message + kGet);
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.consumed, message.size());
    EXPECT_EQ(result.request.body, "data");
}
TEST(HttpAdmission, HeaderExactBudget) {
    const std::string prefix = "GET / HTTP/1.1\r\nHost: local.test\r\nX-Pad: ";
    const std::string suffix = "\r\n\r\n";
    const auto message = prefix + std::string(kAdmissionHeaderBytes - prefix.size() - suffix.size(), 'a') + suffix;
    ASSERT_EQ(message.size(), kAdmissionHeaderBytes);
    EXPECT_EQ(admit_request(message).status, AdmissionStatus::Accepted);
}
TEST(HttpAdmission, HeaderOverBudget) {
    const auto message = "GET / HTTP/1.1\r\nHost: local.test\r\nX-Pad: " +
                         std::string(kAdmissionHeaderBytes, 'a') + "\r\n\r\n";
    expect_hidden(admit_request(message), AdmissionStatus::HeaderTooLarge);
}
TEST(HttpAdmission, IncompleteHeaderAtBudget) {
    expect_hidden(admit_request(std::string(kAdmissionHeaderBytes, 'a')),
                  AdmissionStatus::HeaderTooLarge);
}
TEST(HttpAdmission, FieldCountAtBudget) {
    const auto result = admit_request(header_with_fields(kAdmissionHeaderFields));
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.request.headers.size(), kAdmissionHeaderFields);
}
TEST(HttpAdmission, FieldCountExceeded) {
    expect_hidden(admit_request(header_with_fields(kAdmissionHeaderFields + 1U)),
                  AdmissionStatus::HeaderTooLarge);
}
TEST(HttpAdmission, DeclaredBodyExceedsPolicyBeforeArrival) {
    expect_hidden(admit_request(post(kAdmissionBodyBytes + 1U)), AdmissionStatus::PayloadTooLarge);
}
TEST(HttpAdmission, BodyAtPolicy) {
    const auto message = post(kAdmissionBodyBytes) + std::string(kAdmissionBodyBytes, 'x');
    const auto result = admit_request(message);
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.request.body.size(), kAdmissionBodyBytes);
    EXPECT_EQ(result.consumed, message.size());
}
TEST(HttpAdmission, ChunkedPartialHiddenAndComplete) {
    const auto partial = chunked() + "4\r\nda";
    expect_hidden(admit_request(partial), AdmissionStatus::NeedMoreData);
    const auto message = partial + "ta\r\n0\r\n\r\n";
    const auto result = admit_request(message + kGet);
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.consumed, message.size());
    EXPECT_EQ(result.request.body, "data");
}
TEST(HttpAdmission, DecodedChunkExceedsPolicy) {
    // Test the policy with an in-memory well-formed oversized body, no network.
    const auto message = chunked() + "10001\r\n" + std::string(kAdmissionBodyBytes + 1U, 'b') +
                         "\r\n0\r\n\r\n";
    expect_hidden(admit_request(message), AdmissionStatus::PayloadTooLarge);
}
TEST(HttpAdmission, WireBudgetForIncompleteChunk) {
    const auto prefix = chunked() + "20000\r\n";
    const auto message = prefix + std::string(kAdmissionWireBytes - prefix.size(), 'c');
    ASSERT_EQ(message.size(), kAdmissionWireBytes);
    expect_hidden(admit_request(message), AdmissionStatus::PayloadTooLarge);
}
TEST(HttpAdmission, CompletedFirstRequestPreservesLargeSuffix) {
    const auto result = admit_request(kGet + std::string(kAdmissionWireBytes + 1U, 's'));
    EXPECT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(result.consumed, kGet.size());
}
TEST(HttpAdmission, RejectionsDeterministicAndExposeNoPartialRequest) {
    const auto message = post(kAdmissionBodyBytes + 1U);
    for (unsigned i = 0; i < 4; ++i) {
        expect_hidden(admit_request(message), AdmissionStatus::PayloadTooLarge);
    }
}
}  // namespace
