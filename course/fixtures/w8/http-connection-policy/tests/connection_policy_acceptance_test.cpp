#include "http_message.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <utility>

using namespace cppapplab::http;

namespace {

HttpRequest request_with_version(std::string version) {
    HttpRequest request;
    request.version = std::move(version);
    return request;
}

std::size_t occurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0U;
    std::size_t position = 0U;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

}  // namespace

TEST(HttpConnectionPolicyAcceptanceTest, Http11DefaultsToPersistentConnection) {
    const auto request = request_with_version("HTTP/1.1");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::KeepAlive);
}

TEST(HttpConnectionPolicyAcceptanceTest, Http11CloseTokenOverridesPersistentDefault) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("connection", "Upgrade, KEEP-ALIVE, CLOSE");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, Http11MatchesExactTokensNotSubstrings) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("connection", "x-close, keep-alive-extra");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::KeepAlive);
}

TEST(HttpConnectionPolicyAcceptanceTest, Http10DefaultsToClose) {
    const auto request = request_with_version("HTTP/1.0");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, Http10RequiresExplicitKeepAliveOption) {
    auto request = request_with_version("HTTP/1.0");
    request.headers.emplace("connection", "keep-alive");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::KeepAlive);
}

TEST(HttpConnectionPolicyAcceptanceTest, CloseWinsWhenBothOptionsArePresent) {
    auto request = request_with_version("HTTP/1.0");
    request.headers.emplace("connection", "keep-alive, CLOSE");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, UnsupportedVersionDefaultsToClose) {
    const auto request = request_with_version("HTTP/2.0");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, HeaderNameMatchingIsCaseInsensitive) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("Connection", "close");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, TrimsOptionalWhitespaceAroundTokens) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("connection", "\tupgrade \t, \tclose \t");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, CloseWinsAcrossCaseVariantHeaderEntries) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("Connection", "keep-alive");
    request.headers.emplace("connection", "close");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::Close);
}

TEST(HttpConnectionPolicyAcceptanceTest, EmptyConnectionValueKeepsHttp11Default) {
    auto request = request_with_version("HTTP/1.1");
    request.headers.emplace("connection", " , \t, ");
    EXPECT_EQ(request_connection_policy(request), ConnectionPolicy::KeepAlive);
}

TEST(HttpConnectionPolicyAcceptanceTest, ExplicitKeepAliveSerializesExactlyOnePolicyHeader) {
    HttpResponse response;
    response.headers.emplace("Connection", "close");
    response.body = "ok";

    const std::string wire = serialize_response(response, ConnectionPolicy::KeepAlive);
    EXPECT_EQ(occurrences(wire, "Connection:"), 1U);
    EXPECT_NE(wire.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_NE(wire.find("Content-Length: 2\r\n"), std::string::npos);
}

TEST(HttpConnectionPolicyAcceptanceTest, ExplicitCloseOverridesCallerSuppliedKeepAlive) {
    HttpResponse response;
    response.headers.emplace("connection", "keep-alive");

    const std::string wire = serialize_response(response, ConnectionPolicy::Close);
    EXPECT_EQ(occurrences(wire, "Connection:"), 1U);
    EXPECT_NE(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("Connection: keep-alive\r\n"), std::string::npos);
}

TEST(HttpConnectionPolicyAcceptanceTest, LegacySerializerOverloadStillCloses) {
    HttpResponse response;
    response.headers.emplace("Connection", "keep-alive");

    const std::string wire = serialize_response(response);
    EXPECT_NE(wire.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(wire.find("Connection: keep-alive\r\n"), std::string::npos);
}
