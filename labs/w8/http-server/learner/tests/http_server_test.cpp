#include "http_server.hpp"

#include <gtest/gtest.h>

using namespace cppapplab::http;

TEST(HttpServerLearnerTest, BindsAnEphemeralPort) {
    FAIL() << "TODO(D5-TEST-1): verify the kernel-assigned listening port.";
}
TEST(HttpServerLearnerTest, RejectsInvalidLimits) {
    FAIL() << "TODO(D5-TEST-2): reject invalid worker, queue and connection limits.";
}
TEST(HttpServerLearnerTest, RejectsInvalidBindAddress) {
    FAIL() << "TODO(D5-TEST-3): validate the IPv4 bind address.";
}
TEST(HttpServerLearnerTest, RunsAndStopsThroughEventFd) {
    FAIL() << "TODO(D5-TEST-4): verify eventfd wakes a blocked epoll loop.";
}
TEST(HttpServerLearnerTest, ServesDynamicHello) {
    FAIL() << "TODO(D5-TEST-5): serve GET /hello over a real TCP socket.";
}
TEST(HttpServerLearnerTest, EchoesBinaryRequestBody) {
    FAIL() << "TODO(D5-TEST-6): preserve binary request-body bytes.";
}
TEST(HttpServerLearnerTest, ServesStaticFiles) {
    FAIL() << "TODO(D5-TEST-7): serve a file from the configured document root.";
}
TEST(HttpServerLearnerTest, ConvertsHandlerExceptionsTo500) {
    FAIL() << "TODO(D5-TEST-8): contain worker exceptions and return HTTP 500.";
}
TEST(HttpServerLearnerTest, RejectsMalformedAndOversizedRequests) {
    FAIL() << "TODO(D5-TEST-9): return 400 and 413 for invalid requests.";
}
TEST(HttpServerLearnerTest, RejectsOverloadWith503) {
    FAIL() << "TODO(D5-TEST-10): enforce the maximum active connection limit.";
}
