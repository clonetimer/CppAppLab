#include "router.hpp"
#include "api_handlers.hpp"

#include <gtest/gtest.h>

TEST(RouterLearnerTest, RegistersAndInvokesHandler) {
    FAIL() << "TODO: register GET /hello and verify its exact response.";
}

TEST(RouterLearnerTest, DistinguishesMethods) {
    FAIL() << "TODO: GET and POST on the same path must not be confused.";
}

TEST(RouterLearnerTest, IgnoresQueryForMatching) {
    FAIL() << "TODO: /hello?x=1 must match /hello.";
}

TEST(RouterLearnerTest, ReplacesSameRoute) {
    FAIL() << "TODO: replacing the same method/path must invoke the new callback.";
}

TEST(RouterLearnerTest, EchoesBinaryBody) {
    FAIL() << "TODO: echo a body containing an embedded NUL byte.";
}

TEST(RouterLearnerTest, HealthUsesJsonMimeType) {
    FAIL() << "TODO: verify the health JSON and application/json header.";
}

TEST(RouterLearnerTest, RegistersDefaultEndpoints) {
    FAIL() << "TODO: verify all three default dynamic endpoints are registered.";
}

TEST(RouterLearnerTest, RejectsEmptyCallback) {
    FAIL() << "TODO: invalid replacement must not erase an existing valid route.";
}

TEST(RouterLearnerTest, UnknownDynamicPathReturns404) {
    FAIL() << "TODO: distinguish a missing path from a method mismatch.";
}

TEST(RouterLearnerTest, MethodErrorIncludesAllow) {
    FAIL() << "TODO: known path plus wrong method returns 405 and Allow.";
}
