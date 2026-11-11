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
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace cppapplab::http;

namespace {

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
        timeout.tv_sec = 4;
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
        char buffer[4096];
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
        char buffer[4096];
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
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    if (!predicate()) {
        throw std::runtime_error("timed out waiting for observability state");
    }
}

}  // namespace

TEST(HttpObservabilityAcceptanceTest, EmitsStructuredAccessLogAndResponseCounters) {
    std::mutex log_mutex;
    std::vector<std::string> logs;
    HttpServerConfig config;
    config.log_sink = [&log_mutex, &logs](std::string_view line) {
        const std::lock_guard<std::mutex> lock(log_mutex);
        logs.emplace_back(line);
    };
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello")));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "Hello from C++ HTTP Server!\n");
    EXPECT_TRUE(client.read_until_eof().empty());

    wait_until([&] {
        const std::lock_guard<std::mutex> lock(log_mutex);
        return std::any_of(logs.begin(), logs.end(), [](const std::string& line) {
            return line.find("\"event\":\"http_access\"") != std::string::npos;
        });
    });

    std::string access_log;
    {
        const std::lock_guard<std::mutex> lock(log_mutex);
        const auto found = std::find_if(logs.begin(), logs.end(), [](const std::string& line) {
            return line.find("\"event\":\"http_access\"") != std::string::npos;
        });
        ASSERT_NE(found, logs.end());
        access_log = *found;
    }
    EXPECT_EQ(access_log.front(), '{');
    EXPECT_NE(access_log.find("\"method\":\"GET\""), std::string::npos);
    EXPECT_NE(access_log.find("\"target\":\"/hello\""), std::string::npos);
    EXPECT_NE(access_log.find("\"status\":200"), std::string::npos);
    EXPECT_NE(access_log.find("\"response_bytes\":"), std::string::npos);
    EXPECT_NE(access_log.find("\"connection\":\"close\""), std::string::npos);

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.accepted_connections, 1U);
    EXPECT_EQ(metrics.rejected_connections, 0U);
    EXPECT_EQ(metrics.active_connections, 0U);
    EXPECT_EQ(metrics.closed_connections, 1U);
    EXPECT_EQ(metrics.requests_received, 1U);
    EXPECT_EQ(metrics.responses_completed, 1U);
    EXPECT_GT(metrics.response_bytes, 0U);
    EXPECT_EQ(metrics.client_errors, 0U);
    EXPECT_EQ(metrics.server_errors, 0U);
    EXPECT_EQ(metrics.active_jobs, 0U);
}

TEST(HttpObservabilityAcceptanceTest, TracksPersistentAndActiveConnectionGauges) {
    TestServer server;
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello", "keep-alive")));
    const std::string first = client.read_response();
    ASSERT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);

    HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.accepted_connections, 1U);
    EXPECT_EQ(metrics.active_connections, 1U);
    EXPECT_EQ(metrics.persistent_connections, 1U);
    EXPECT_EQ(metrics.requests_received, 1U);
    EXPECT_EQ(metrics.responses_completed, 1U);

    ASSERT_TRUE(client.send_all(make_request("/health")));
    const std::string second = client.read_response();
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_TRUE(client.read_until_eof().empty());

    metrics = server.metrics();
    EXPECT_EQ(metrics.active_connections, 0U);
    EXPECT_EQ(metrics.persistent_connections, 0U);
    EXPECT_EQ(metrics.closed_connections, 1U);
    EXPECT_EQ(metrics.requests_received, 2U);
    EXPECT_EQ(metrics.responses_completed, 2U);
}

TEST(HttpObservabilityAcceptanceTest, HandlerFailureProducesErrorLogAndServerErrorMetrics) {
    std::mutex log_mutex;
    std::vector<std::string> logs;
    Router router = make_default_router();
    router.add_route("GET", "/explode", [](const HttpRequest&) -> HttpResponse {
        throw std::runtime_error("do not log raw exception text");
    });
    HttpServerConfig config;
    config.log_sink = [&log_mutex, &logs](std::string_view line) {
        const std::lock_guard<std::mutex> lock(log_mutex);
        logs.emplace_back(line);
    };
    TestServer server(config, std::move(router));
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/explode")));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 500 Internal Server Error\r\n"), std::string::npos);
    EXPECT_TRUE(client.read_until_eof().empty());

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.handler_failures, 1U);
    EXPECT_EQ(metrics.server_errors, 1U);
    EXPECT_EQ(metrics.responses_completed, 1U);

    const std::lock_guard<std::mutex> lock(log_mutex);
    EXPECT_TRUE(std::any_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("\"event\":\"handler_failure\"") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("\"event\":\"http_access\"") != std::string::npos &&
               line.find("\"status\":500") != std::string::npos;
    }));
    EXPECT_TRUE(std::none_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("do not log raw exception text") != std::string::npos;
    }));
}

TEST(HttpObservabilityAcceptanceTest, ErrorLogCanBeDisabledWithoutDisablingAccessLog) {
    std::mutex log_mutex;
    std::vector<std::string> logs;
    HttpServerConfig config;
    config.error_log_enabled = false;
    config.log_sink = [&log_mutex, &logs](std::string_view line) {
        const std::lock_guard<std::mutex> lock(log_mutex);
        logs.emplace_back(line);
    };
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/missing?token=private-value")));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 404 Not Found\r\n"), std::string::npos);
    EXPECT_TRUE(client.read_until_eof().empty());

    const std::lock_guard<std::mutex> lock(log_mutex);
    EXPECT_TRUE(std::any_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("\"event\":\"http_access\"") != std::string::npos &&
               line.find("\"target\":\"/missing\"") != std::string::npos &&
               line.find("\"query_present\":true") != std::string::npos;
    }));
    EXPECT_TRUE(std::none_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("\"event\":\"http_error\"") != std::string::npos;
    }));
}

TEST(HttpObservabilityAcceptanceTest, LogSinkExceptionsDoNotBreakRequestProcessing) {
    HttpServerConfig config;
    config.log_sink = [](std::string_view) {
        throw std::runtime_error("log sink unavailable");
    };
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello")));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 200 OK\\r\\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "Hello from C++ HTTP Server!\\n");
    EXPECT_TRUE(client.read_until_eof().empty());

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_GE(metrics.log_sink_failures, 2U);
    EXPECT_EQ(metrics.responses_completed, 1U);
}

TEST(HttpObservabilityAcceptanceTest, TracksIdleAndRequestTimeoutCounters) {
    HttpServerConfig config;
    config.idle_timeout = 100ms;
    config.header_timeout = 120ms;
    config.body_timeout = 2s;
    config.error_log_enabled = false;
    TestServer server(config);

    TestClient idle_client(server.port());
    EXPECT_TRUE(idle_client.read_until_eof().empty());

    TestClient partial_client(server.port());
    ASSERT_TRUE(partial_client.send_all("GET /hello HTTP/1.1\r\nHost: localhost\r\nX-Partial:"));
    const std::string response = partial_client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 408 Request Timeout\r\n"), std::string::npos);
    EXPECT_TRUE(partial_client.read_until_eof().empty());

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.accepted_connections, 2U);
    EXPECT_EQ(metrics.idle_timeouts, 1U);
    EXPECT_EQ(metrics.request_timeouts, 1U);
    EXPECT_EQ(metrics.client_errors, 1U);
    EXPECT_EQ(metrics.responses_completed, 1U);
}

TEST(HttpObservabilityAcceptanceTest, CountsConnectionsRejectedByMaximumConnectionLimit) {
    HttpServerConfig config;
    config.max_connections = 1U;
    config.idle_timeout = 2s;
    config.error_log_enabled = false;
    TestServer server(config);
    TestClient first(server.port());
    TestClient second(server.port());

    const std::string rejected = second.read_until_eof();
    EXPECT_NE(rejected.find("HTTP/1.1 503 Service Unavailable\r\n"), std::string::npos);

    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.accepted_connections, 1U);
    EXPECT_EQ(metrics.rejected_connections, 1U);
    EXPECT_EQ(metrics.active_connections, 1U);
}

TEST(HttpObservabilityAcceptanceTest, CountsWorkerQueueRejections) {
    std::atomic<bool> entered{false};
    Router router = make_default_router();
    router.add_route("GET", "/slow", [&entered](const HttpRequest&) {
        entered.store(true);
        std::this_thread::sleep_for(200ms);
        HttpResponse response;
        response.body = "finished";
        return response;
    });
    HttpServerConfig config;
    config.worker_count = 1U;
    config.task_queue_capacity = 1U;
    config.max_connections = 4U;
    config.error_log_enabled = false;
    TestServer server(config, std::move(router));
    TestClient slow(server.port());

    ASSERT_TRUE(slow.send_all(make_request("/slow")));
    wait_until([&] { return entered.load(); });

    TestClient rejected(server.port());
    ASSERT_TRUE(rejected.send_all(make_request("/health")));
    const std::string response = rejected.read_response();
    EXPECT_NE(response.find("HTTP/1.1 503 Service Unavailable\r\n"), std::string::npos);
    EXPECT_TRUE(rejected.read_until_eof().empty());

    EXPECT_EQ(response_body(slow.read_response()), "finished");
    EXPECT_TRUE(slow.read_until_eof().empty());
    const HttpServerMetrics metrics = server.metrics();
    EXPECT_EQ(metrics.queue_rejections, 1U);
    EXPECT_EQ(metrics.active_jobs, 0U);
}
