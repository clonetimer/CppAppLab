#include "http_server.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace cppapplab::http;

namespace {

constexpr std::size_t kBytesPerKiB = 1024U;
constexpr std::size_t kBytesPerMiB = kBytesPerKiB * kBytesPerKiB;
constexpr std::size_t kSlowReaderPayloadMiB = 8U;
constexpr std::size_t kSlowReaderResponseLimitMiB = 10U;
constexpr int kSlowReaderReceiveBufferBytes = 64 * static_cast<int>(kBytesPerKiB);

class TestServer {
   public:
    explicit TestServer(HttpServerConfig config = {}, Router router = make_default_router())
        : server_([&] {
              config.port = 0U;
              return HttpServer(std::move(config), std::move(router));
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

    void request_stop() noexcept {
        server_.request_stop();
    }

    void join() {
        if (runner_.joinable()) {
            runner_.join();
        }
    }

   private:
    HttpServer server_;
    std::thread runner_;
};

class TestClient {
   public:
    explicit TestClient(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) {
            throw std::runtime_error("socket failed");
        }

        timeval timeout{};
        timeout.tv_sec = 5;
        (void)::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
            (void)::close(fd_);
            fd_ = -1;
            throw std::runtime_error("connect failed");
        }
    }

    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;

    ~TestClient() {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }

    void set_receive_buffer(int bytes) const {
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes)) < 0) {
            throw std::runtime_error("setsockopt(SO_RCVBUF) failed");
        }
    }

    [[nodiscard]] bool send_all(std::string_view bytes) const {
        std::size_t offset = 0U;
        while (offset < bytes.size()) {
            const ssize_t sent =
                ::send(fd_, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL);
            if (sent > 0) {
                offset += static_cast<std::size_t>(sent);
            } else if (sent < 0 && errno == EINTR) {
                continue;
            } else {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string read_response() {
        std::size_t header_end = read_buffer_.find("\r\n\r\n");
        while (header_end == std::string::npos) {
            receive_more();
            header_end = read_buffer_.find("\r\n\r\n");
        }

        const std::string headers = read_buffer_.substr(0U, header_end);
        const std::string prefix = "Content-Length: ";
        const std::size_t position = headers.find(prefix);
        if (position == std::string::npos) {
            throw std::runtime_error("response is missing Content-Length");
        }
        const std::size_t value_begin = position + prefix.size();
        const std::size_t line_end = headers.find("\r\n", value_begin);
        const std::string length_text =
            headers.substr(value_begin, line_end == std::string::npos ? std::string::npos
                                                                      : line_end - value_begin);
        const std::size_t body_size = static_cast<std::size_t>(std::stoull(length_text));
        const std::size_t response_size = header_end + 4U + body_size;
        while (read_buffer_.size() < response_size) {
            receive_more();
        }

        std::string response = read_buffer_.substr(0U, response_size);
        read_buffer_.erase(0U, response_size);
        return response;
    }

    [[nodiscard]] std::string read_until_eof() {
        std::string result = std::move(read_buffer_);
        read_buffer_.clear();
        char buffer[16384];
        for (;;) {
            const ssize_t received = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (received > 0) {
                result.append(buffer, static_cast<std::size_t>(received));
            } else if (received < 0 && errno == EINTR) {
                continue;
            } else if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                throw std::runtime_error("timed out waiting for connection close");
            } else {
                break;
            }
        }
        return result;
    }

   private:
    void receive_more() {
        char buffer[16384];
        for (;;) {
            const ssize_t received = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (received > 0) {
                read_buffer_.append(buffer, static_cast<std::size_t>(received));
                return;
            }
            if (received < 0 && errno == EINTR) {
                continue;
            }
            if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                throw std::runtime_error("timed out waiting for HTTP response bytes");
            }
            throw std::runtime_error("connection closed before a complete HTTP response");
        }
    }

    int fd_{-1};
    std::string read_buffer_;
};

[[nodiscard]] std::string make_request(std::string_view target,
                                       std::string_view connection = "close") {
    std::string wire = "GET ";
    wire.append(target.data(), target.size());
    wire += " HTTP/1.1\r\nHost: localhost\r\nConnection: ";
    wire.append(connection.data(), connection.size());
    wire += "\r\n\r\n";
    return wire;
}

[[nodiscard]] std::string response_body(const std::string& response) {
    const std::size_t split = response.find("\r\n\r\n");
    return split == std::string::npos ? std::string{} : response.substr(split + 4U);
}

template <typename Predicate>
void wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    if (!predicate()) {
        throw std::runtime_error("timed out waiting for HTTP server state");
    }
}

[[nodiscard]] bool perform_concurrent_work_request(std::uint16_t port) {
    TestClient client(port);
    if (!client.send_all(make_request("/work"))) {
        return false;
    }
    const std::string response = client.read_response();
    return response.find("HTTP/1.1 200 OK\r\n") != std::string::npos &&
           response_body(response) == "worked" && client.read_until_eof().empty();
}

[[nodiscard]] bool perform_keep_alive_soak_client(std::uint16_t port, int request_count) {
    TestClient client(port);
    for (int request_index = 0; request_index < request_count; ++request_index) {
        const std::string policy = request_index + 1 == request_count ? "close" : "keep-alive";
        if (!client.send_all(make_request("/health", policy))) {
            return false;
        }
        const std::string response = client.read_response();
        if (response.find("HTTP/1.1 200 OK\r\n") == std::string::npos ||
            response_body(response) != "{\"status\":\"ok\"}") {
            return false;
        }
    }
    return client.read_until_eof().empty();
}

[[nodiscard]] bool perform_server_lifecycle_cycle() {
    TestServer server;
    TestClient client(server.port());
    if (!client.send_all(make_request("/health"))) {
        return false;
    }
    const std::string response = client.read_response();
    if (response.find("HTTP/1.1 200 OK\r\n") == std::string::npos ||
        response_body(response) != "{\"status\":\"ok\"}" || !client.read_until_eof().empty()) {
        return false;
    }

    server.request_stop();
    server.request_stop();
    server.join();

    const HttpServerMetrics metrics = server.metrics();
    return metrics.graceful_shutdowns == 1U && metrics.active_connections == 0U &&
           metrics.active_jobs == 0U && metrics.responses_completed == 1U &&
           metrics.closed_connections == 1U;
}

}  // namespace

TEST(HttpOperationalAcceptanceTest, ServesConcurrentClientsWithinWorkerAndQueueBounds) {
    constexpr int kClientCount = 16;
    constexpr int kWorkerCount = 4;
    constexpr int kMinimumConcurrentHandlers = 2;
    constexpr std::size_t kTaskQueueCapacity = 32U;
    constexpr std::size_t kMaximumConnections = 32U;
    std::atomic<int> active_handlers{0};
    std::atomic<int> maximum_active_handlers{0};

    Router router = make_default_router();
    router.add_route("GET", "/work",
                     [&active_handlers, &maximum_active_handlers](const HttpRequest&) {
                         const int active = active_handlers.fetch_add(1) + 1;
                         int observed = maximum_active_handlers.load();
                         while (observed < active &&
                                !maximum_active_handlers.compare_exchange_weak(observed, active)) {
                         }
                         std::this_thread::sleep_for(40ms);
                         active_handlers.fetch_sub(1);
                         HttpResponse response;
                         response.body = "worked";
                         return response;
                     });

    HttpServerConfig config;
    config.worker_count = static_cast<std::size_t>(kWorkerCount);
    config.task_queue_capacity = kTaskQueueCapacity;
    config.max_connections = kMaximumConnections;
    config.access_log_enabled = false;
    config.error_log_enabled = false;
    TestServer server(config, std::move(router));

    std::atomic<bool> start{false};
    std::atomic<int> succeeded{0};
    std::atomic<int> failed{0};
    std::vector<std::thread> clients;
    clients.reserve(static_cast<std::size_t>(kClientCount));
    for (int index = 0; index < kClientCount; ++index) {
        clients.emplace_back([&] {
            while (!start.load()) {
                std::this_thread::yield();
            }
            try {
                if (perform_concurrent_work_request(server.port())) {
                    succeeded.fetch_add(1);
                } else {
                    failed.fetch_add(1);
                }
            } catch (const std::exception&) {
                failed.fetch_add(1);
            }
        });
    }

    start.store(true);
    for (auto& client : clients) {
        client.join();
    }

    EXPECT_EQ(succeeded.load(), kClientCount);
    EXPECT_EQ(failed.load(), 0);
    EXPECT_GE(maximum_active_handlers.load(), kMinimumConcurrentHandlers);
    EXPECT_LE(maximum_active_handlers.load(), kWorkerCount);
    wait_until([&] {
        return server.metrics().responses_completed == static_cast<std::uint64_t>(kClientCount);
    });

    const HttpServerMetrics metrics = server.metrics();
    const auto expected_clients = static_cast<std::uint64_t>(kClientCount);
    EXPECT_EQ(metrics.accepted_connections, expected_clients);
    EXPECT_EQ(metrics.closed_connections, expected_clients);
    EXPECT_EQ(metrics.requests_received, expected_clients);
    EXPECT_EQ(metrics.responses_completed, expected_clients);
    EXPECT_EQ(metrics.queue_rejections, 0U);
    EXPECT_EQ(metrics.active_connections, 0U);
    EXPECT_EQ(metrics.active_jobs, 0U);
}

TEST(HttpOperationalAcceptanceTest, SlowReaderBackpressureResumesAndCompletesLargeResponse) {
    constexpr std::size_t kPayloadSize = kSlowReaderPayloadMiB * kBytesPerMiB;
    constexpr std::size_t kMaximumResponseSize = kSlowReaderResponseLimitMiB * kBytesPerMiB;
    const auto payload = std::make_shared<const std::string>(kPayloadSize, 'x');

    Router router = make_default_router();
    router.add_route("GET", "/large", [payload](const HttpRequest&) {
        HttpResponse response;
        response.headers.emplace("Content-Type", "application/octet-stream");
        response.body = *payload;
        return response;
    });

    HttpServerConfig config;
    config.max_response_bytes = kMaximumResponseSize;
    config.access_log_enabled = false;
    config.error_log_enabled = false;
    TestServer server(config, std::move(router));
    TestClient client(server.port());
    client.set_receive_buffer(kSlowReaderReceiveBufferBytes);

    ASSERT_TRUE(client.send_all(make_request("/large")));
    std::this_thread::sleep_for(150ms);
    EXPECT_EQ(server.metrics().responses_completed, 0U);

    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    const std::string body = response_body(response);
    ASSERT_EQ(body.size(), kPayloadSize);
    EXPECT_EQ(body.front(), 'x');
    EXPECT_EQ(body.back(), 'x');
    EXPECT_TRUE(client.read_until_eof().empty());

    wait_until([&] { return server.metrics().responses_completed == 1U; });
    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.response_bytes, static_cast<std::uint64_t>(response.size()));
    EXPECT_EQ(metrics.server_errors, 0U);
    EXPECT_EQ(metrics.active_connections, 0U);
    EXPECT_EQ(metrics.active_jobs, 0U);
}

TEST(HttpOperationalAcceptanceTest, SustainsConcurrentKeepAliveSoakWithoutLeakingResources) {
    constexpr int kClientCount = 8;
    constexpr int kRequestsPerClient = 40;
    constexpr std::size_t kWorkerCount = 4U;
    constexpr std::size_t kTaskQueueCapacity = 32U;
    constexpr std::size_t kMaximumConnections = 16U;
    constexpr std::size_t kMaximumPersistentConnections = 16U;
    constexpr std::size_t kMaximumRequestsPerConnection = 100U;
    constexpr std::uint64_t kExpectedRequests =
        static_cast<std::uint64_t>(kClientCount) * static_cast<std::uint64_t>(kRequestsPerClient);

    HttpServerConfig config;
    config.worker_count = kWorkerCount;
    config.task_queue_capacity = kTaskQueueCapacity;
    config.max_connections = kMaximumConnections;
    config.max_persistent_connections = kMaximumPersistentConnections;
    config.max_requests_per_connection = kMaximumRequestsPerConnection;
    config.access_log_enabled = false;
    config.error_log_enabled = false;
    TestServer server(config);

    std::atomic<int> succeeded{0};
    std::atomic<int> failed{0};
    std::vector<std::thread> clients;
    clients.reserve(static_cast<std::size_t>(kClientCount));
    for (int index = 0; index < kClientCount; ++index) {
        clients.emplace_back([&] {
            try {
                if (perform_keep_alive_soak_client(server.port(), kRequestsPerClient)) {
                    succeeded.fetch_add(1);
                } else {
                    failed.fetch_add(1);
                }
            } catch (const std::exception&) {
                failed.fetch_add(1);
            }
        });
    }
    for (auto& client : clients) {
        client.join();
    }

    EXPECT_EQ(succeeded.load(), kClientCount);
    EXPECT_EQ(failed.load(), 0);
    wait_until([&] {
        const HttpServerMetrics metrics = server.metrics();
        return metrics.responses_completed == kExpectedRequests &&
               metrics.active_connections == 0U && metrics.active_jobs == 0U;
    });

    const HttpServerMetrics metrics = server.metrics();
    const auto expected_clients = static_cast<std::uint64_t>(kClientCount);
    EXPECT_EQ(metrics.accepted_connections, expected_clients);
    EXPECT_EQ(metrics.closed_connections, expected_clients);
    EXPECT_EQ(metrics.requests_received, kExpectedRequests);
    EXPECT_EQ(metrics.responses_completed, kExpectedRequests);
    EXPECT_EQ(metrics.persistent_connections, 0U);
    EXPECT_EQ(metrics.queue_rejections, 0U);
    EXPECT_EQ(metrics.client_errors, 0U);
    EXPECT_EQ(metrics.server_errors, 0U);
}

TEST(HttpOperationalAcceptanceTest, RepeatedRunStopCyclesReleaseConnectionsAndWorkerJobs) {
    constexpr int kCycles = 8;
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        EXPECT_TRUE(perform_server_lifecycle_cycle()) << "cycle " << cycle;
    }
}
