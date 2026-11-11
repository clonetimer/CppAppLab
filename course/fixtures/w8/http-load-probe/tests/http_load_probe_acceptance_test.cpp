#include "http_load_probe.hpp"
#include "http_server.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;
using namespace cppapplab::http;

namespace {

class TestServer {
   public:
    TestServer()
        : server_([] {
              HttpServerConfig config;
              config.port = 0U;
              config.access_log_enabled = false;
              config.error_log_enabled = false;
              return HttpServer(config);
          }()) {
        runner_ = std::thread([this] { server_.run(); });
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (!server_.running() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        if (!server_.running()) {
            server_.request_stop();
            runner_.join();
            throw std::runtime_error("HTTP server did not start");
        }
    }

    ~TestServer() {
        if (runner_.joinable()) {
            server_.request_stop();
            runner_.join();
        }
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return server_.port();
    }

    [[nodiscard]] HttpServerMetrics metrics() const noexcept {
        return server_.metrics();
    }

   private:
    HttpServer server_;
    std::thread runner_;
};

[[nodiscard]] std::uint16_t unused_loopback_port() {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw std::runtime_error("socket failed");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0U;
    (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        (void)::close(fd);
        throw std::runtime_error("bind failed");
    }
    socklen_t length = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        (void)::close(fd);
        throw std::runtime_error("getsockname failed");
    }
    const std::uint16_t port = ntohs(address.sin_port);
    (void)::close(fd);
    return port;
}

void wait_for_responses(const TestServer& server, std::uint64_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (server.metrics().responses_completed < expected &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
}

}  // namespace

TEST(HttpLoadProbeAcceptanceTest, RejectsInvalidConfiguration) {
    HttpLoadProbeConfig config;
    EXPECT_THROW(run_http_load_probe(config), std::invalid_argument);

    config.port = 8080U;
    config.client_count = 0U;
    EXPECT_THROW(run_http_load_probe(config), std::invalid_argument);

    config.client_count = 1U;
    config.requests_per_client = 0U;
    EXPECT_THROW(run_http_load_probe(config), std::invalid_argument);

    config.requests_per_client = 1U;
    config.target = "health";
    EXPECT_THROW(run_http_load_probe(config), std::invalid_argument);

    config.target = "/health";
    config.timeout = 0ms;
    EXPECT_THROW(run_http_load_probe(config), std::invalid_argument);
}

TEST(HttpLoadProbeAcceptanceTest, ReportsSingleClientLatencyAndThroughput) {
    TestServer server;
    HttpLoadProbeConfig config;
    config.port = server.port();
    config.client_count = 1U;
    config.requests_per_client = 5U;

    const HttpLoadProbeResult result = run_http_load_probe(config);
    EXPECT_EQ(result.requests_attempted, 5U);
    EXPECT_EQ(result.requests_succeeded, 5U);
    EXPECT_EQ(result.requests_failed, 0U);
    EXPECT_GT(result.elapsed_microseconds, 0U);
    EXPECT_GT(result.requests_per_second, 0.0);
    EXPECT_GT(result.p50_latency_microseconds, 0U);
    EXPECT_LE(result.p50_latency_microseconds, result.p95_latency_microseconds);
    EXPECT_LE(result.p95_latency_microseconds, result.p99_latency_microseconds);
    wait_for_responses(server, 5U);
    EXPECT_EQ(server.metrics().responses_completed, 5U);
}

TEST(HttpLoadProbeAcceptanceTest, RunsConcurrentKeepAliveClientsAndAccountsForResources) {
    constexpr std::size_t kClients = 6U;
    constexpr std::size_t kRequestsPerClient = 8U;
    constexpr std::uint64_t kRequests = kClients * kRequestsPerClient;
    TestServer server;
    HttpLoadProbeConfig config;
    config.port = server.port();
    config.client_count = kClients;
    config.requests_per_client = kRequestsPerClient;

    const HttpLoadProbeResult result = run_http_load_probe(config);
    EXPECT_EQ(result.requests_attempted, kRequests);
    EXPECT_EQ(result.requests_succeeded, kRequests);
    EXPECT_EQ(result.requests_failed, 0U);
    wait_for_responses(server, kRequests);

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.accepted_connections, kClients);
    EXPECT_EQ(metrics.closed_connections, kClients);
    EXPECT_EQ(metrics.requests_received, kRequests);
    EXPECT_EQ(metrics.responses_completed, kRequests);
    EXPECT_EQ(metrics.active_connections, 0U);
    EXPECT_EQ(metrics.persistent_connections, 0U);
    EXPECT_EQ(metrics.active_jobs, 0U);
}

TEST(HttpLoadProbeAcceptanceTest, CountsUnexpectedStatusAsFailedRequests) {
    TestServer server;
    HttpLoadProbeConfig config;
    config.port = server.port();
    config.target = "/not-found";
    config.client_count = 2U;
    config.requests_per_client = 3U;
    config.expected_status_code = 200;

    const HttpLoadProbeResult result = run_http_load_probe(config);
    EXPECT_EQ(result.requests_attempted, 6U);
    EXPECT_EQ(result.requests_succeeded, 0U);
    EXPECT_EQ(result.requests_failed, 6U);
}

TEST(HttpLoadProbeAcceptanceTest, CountsConnectionFailuresWithoutThrowing) {
    HttpLoadProbeConfig config;
    config.port = unused_loopback_port();
    config.client_count = 2U;
    config.requests_per_client = 3U;

    const HttpLoadProbeResult result = run_http_load_probe(config);
    EXPECT_EQ(result.requests_attempted, 6U);
    EXPECT_EQ(result.requests_succeeded, 0U);
    EXPECT_EQ(result.requests_failed, 6U);
}
