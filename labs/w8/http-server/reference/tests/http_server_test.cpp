#include "http_server.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace cppapplab::http;

TEST(HttpServerLearnerTest, BindsAnEphemeralPort) {
    HttpServerConfig config;
    config.port = 0U;
    HttpServer server(config);
    EXPECT_NE(server.port(), 0U);
}
TEST(HttpServerLearnerTest, RejectsZeroWorkerCount) {
    HttpServerConfig config;
    config.worker_count = 0U;
    EXPECT_THROW(HttpServer(config), std::invalid_argument);
}
TEST(HttpServerLearnerTest, RejectsZeroQueueCapacity) {
    HttpServerConfig config;
    config.task_queue_capacity = 0U;
    EXPECT_THROW(HttpServer(config), std::invalid_argument);
}
TEST(HttpServerLearnerTest, RequiresQueueCapacityForEveryConnection) {
    HttpServerConfig config;
    config.max_connections = 4U;
    config.task_queue_capacity = 3U;
    EXPECT_THROW(HttpServer(config), std::invalid_argument);
}
TEST(HttpServerLearnerTest, RejectsInvalidBindAddress) {
    HttpServerConfig config;
    config.bind_address = "not-an-ip";
    EXPECT_THROW(HttpServer(config), std::invalid_argument);
}
TEST(HttpServerLearnerTest, RejectsTooSmallResponseLimit) {
    HttpServerConfig config;
    config.max_response_bytes = 128U;
    EXPECT_THROW(HttpServer(config), std::invalid_argument);
}
TEST(HttpServerLearnerTest, ExposesNotRunningBeforeRun) {
    HttpServer server;
    EXPECT_FALSE(server.running());
}
TEST(HttpServerLearnerTest, StopBeforeRunMakesRunReturn) {
    HttpServer server;
    server.request_stop();
    EXPECT_NO_THROW(server.run());
    EXPECT_FALSE(server.running());
}
TEST(HttpServerLearnerTest, RunCanOnlyBeCalledOnce) {
    HttpServer server;
    server.request_stop();
    server.run();
    EXPECT_THROW(server.run(), std::logic_error);
}
TEST(HttpServerLearnerTest, RequestStopIsIdempotent) {
    HttpServer server;
    server.request_stop();
    server.request_stop();
    EXPECT_NO_THROW(server.run());
}
