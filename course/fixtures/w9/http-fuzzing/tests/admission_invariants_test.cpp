#include "admission_replay.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace {
using namespace cppapplab::http;

const std::string kGet = "GET / HTTP/1.1\r\nHost: local.test\r\n\r\n";

AdmissionResult accepted() {
    return admit_request(kGet);
}

TEST(AdmissionOracle, AcceptsActualCompleteRequest) {
    const auto result = accepted();
    ASSERT_EQ(result.status, AdmissionStatus::Accepted);
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::None);
}
TEST(AdmissionOracle, AcceptsActualIncompleteAndRejectedRequests) {
    EXPECT_EQ(validate_observation("", admit_request("")), InvariantViolation::None);
    const std::string malformed = "GET / HTTP/1.1\r\n\r\n";
    EXPECT_EQ(validate_observation(malformed, admit_request(malformed)), InvariantViolation::None);
}
TEST(AdmissionOracle, DetectsUnknownStatus) {
    AdmissionResult result;
    result.status = static_cast<AdmissionStatus>(99);
    EXPECT_EQ(validate_observation("", result), InvariantViolation::UnknownStatus);
}
TEST(AdmissionOracle, DetectsConsumedOnIncomplete) {
    AdmissionResult result;
    result.consumed = 1;
    EXPECT_EQ(validate_observation("a", result), InvariantViolation::HiddenState);
}
TEST(AdmissionOracle, DetectsPartialRequestFields) {
    for (int field = 0; field < 5; ++field) {
        AdmissionResult result;
        if (field == 0) result.request.method = "GET";
        if (field == 1) result.request.target = "/";
        if (field == 2) result.request.version = "HTTP/1.1";
        if (field == 3) result.request.headers.emplace("host", "local.test");
        if (field == 4) result.request.body = "data";
        EXPECT_EQ(validate_observation("", result), InvariantViolation::HiddenState);
    }
}
TEST(AdmissionOracle, DetectsMissingRequiredClose) {
    for (const auto status : {AdmissionStatus::BadRequest, AdmissionStatus::HeaderTooLarge,
                              AdmissionStatus::PayloadTooLarge}) {
        AdmissionResult result;
        result.status = status;
        EXPECT_EQ(validate_observation("", result), InvariantViolation::ClosePolicy);
    }
}
TEST(AdmissionOracle, DetectsUnexpectedClose) {
    AdmissionResult result;
    result.close_connection = true;
    EXPECT_EQ(validate_observation("", result), InvariantViolation::ClosePolicy);
    result = accepted();
    result.close_connection = true;
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::ClosePolicy);
}
TEST(AdmissionOracle, DetectsZeroConsumption) {
    auto result = accepted();
    result.consumed = 0;
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::ConsumedBoundary);
}
TEST(AdmissionOracle, DetectsConsumptionBeyondInput) {
    auto result = accepted();
    result.consumed = kGet.size() + 1;
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::ConsumedBoundary);
}
TEST(AdmissionOracle, DetectsWireLimit) {
    auto result = accepted();
    result.consumed = kAdmissionWireBytes + 1;
    const auto input = kGet + std::string(kAdmissionWireBytes, 'x');
    EXPECT_EQ(validate_observation(input, result), InvariantViolation::ConsumedBoundary);
}
TEST(AdmissionOracle, DetectsIncompleteRequestShape) {
    auto result = accepted();
    result.request.method.clear();
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::RequestShape);
    result = accepted();
    result.request.target.clear();
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::RequestShape);
    result = accepted();
    result.request.version = "unknown";
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::RequestShape);
    result = accepted();
    result.request.headers["host"].clear();
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::RequestShape);
}
TEST(AdmissionOracle, DetectsHeaderBoundary) {
    auto result = accepted();
    result.consumed = kGet.size() - 1;
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::HeaderBoundary);
    const auto long_header = std::string(kAdmissionHeaderBytes, 'a') + "\r\n\r\n";
    result.consumed = long_header.size();
    EXPECT_EQ(validate_observation(long_header, result), InvariantViolation::HeaderBoundary);
}
TEST(AdmissionOracle, DetectsResourceBudget) {
    auto result = accepted();
    for (std::size_t i = 0; i < kAdmissionHeaderFields; ++i) {
        result.request.headers.emplace("x-" + std::to_string(i), "v");
    }
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::ResourceBudget);
    result = accepted();
    result.request.body = "extra";
    EXPECT_EQ(validate_observation(kGet, result), InvariantViolation::ResourceBudget);
    const auto input = kGet + std::string(kAdmissionBodyBytes + 1U, 'b');
    result.consumed = input.size();
    result.request.body = std::string(kAdmissionBodyBytes + 1U, 'b');
    EXPECT_EQ(validate_observation(input, result), InvariantViolation::ResourceBudget);
}
TEST(AdmissionOracle, ComparesStatusBoundaryAndClose) {
    const auto a = accepted();
    auto b = a;
    EXPECT_TRUE(same_observation(a, b));
    b.status = AdmissionStatus::BadRequest;
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    ++b.consumed;
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    b.close_connection = true;
    EXPECT_FALSE(same_observation(a, b));
}
TEST(AdmissionOracle, ComparesAllRequestFields) {
    const auto a = accepted();
    auto b = a;
    b.request.method = "POST";
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    b.request.target = "/next";
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    b.request.version = "HTTP/1.0";
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    b.request.headers["host"] = "other.test";
    EXPECT_FALSE(same_observation(a, b));
    b = a;
    b.request.body = "x";
    EXPECT_FALSE(same_observation(a, b));
}
TEST(AdmissionOracle, HeaderOrderNotSemantic) {
    auto a = accepted();
    auto b = a;
    a.request.headers.clear();
    b.request.headers.clear();
    a.request.headers.emplace("host", "local.test");
    a.request.headers.emplace("x-id", "1");
    b.request.headers.emplace("x-id", "1");
    b.request.headers.emplace("host", "local.test");
    EXPECT_TRUE(same_observation(a, b));
}
TEST(AdmissionReplay, NormalPipelineAndPrefixes) {
    const auto input = kGet + kGet;
    EXPECT_EQ(verify_admission_input(input), InvariantViolation::None);
    for (std::size_t i = 0; i < kGet.size(); ++i) {
        EXPECT_EQ(verify_admission_input(std::string_view(kGet).substr(0, i)),
                  InvariantViolation::None);
    }
}
TEST(AdmissionReplay, BinaryBody) {
    const auto input = std::string("POST / HTTP/1.1\r\nHost: local.test\r\nContent-Length: 3\r\n\r\n") +
                       std::string("a\0b", 3);
    ASSERT_EQ(admit_request(input).status, AdmissionStatus::Accepted);
    EXPECT_EQ(verify_admission_input(input + kGet), InvariantViolation::None);
}
TEST(AdmissionReplay, ChunkedBody) {
    const std::string input =
        "POST / HTTP/1.1\r\nHost: local.test\r\nTransfer-Encoding: chunked\r\n\r\n"
        "4\r\ndata\r\n0\r\n\r\n";
    ASSERT_EQ(admit_request(input).status, AdmissionStatus::Accepted);
    EXPECT_EQ(verify_admission_input(input), InvariantViolation::None);
    EXPECT_EQ(verify_admission_input(input.substr(0, input.size() - 1)), InvariantViolation::None);
}
TEST(AdmissionReplay, BudgetBoundaries) {
    const auto oversized = std::string(kAdmissionHeaderBytes, 'a');
    EXPECT_EQ(verify_admission_input(oversized), InvariantViolation::None);
    const auto body = "POST / HTTP/1.1\r\nHost: local.test\r\nContent-Length: 65536\r\n\r\n" +
                      std::string(kAdmissionBodyBytes, 'b');
    ASSERT_EQ(admit_request(body).status, AdmissionStatus::Accepted);
    EXPECT_EQ(verify_admission_input(body), InvariantViolation::None);
}
TEST(AdmissionReplay, AllSingleBytes) {
    for (unsigned value = 0; value <= 255; ++value) {
        const std::array<char, 1> data{static_cast<char>(value)};
        EXPECT_EQ(verify_admission_input(std::string_view(data.data(), data.size())),
                  InvariantViolation::None);
    }
}
}  // namespace
