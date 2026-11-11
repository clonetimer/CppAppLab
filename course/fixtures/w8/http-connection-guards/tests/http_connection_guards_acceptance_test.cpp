#include "http_server.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace std::chrono_literals;
using namespace cppapplab::http;

namespace {

class TestServer {
   public:
    explicit TestServer(HttpServerConfig config = {})
        : server_([&] {
              config.port = 0U;
              return HttpServer(std::move(config));
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
                                       std::string_view connection = {}) {
    std::string wire = "GET ";
    wire.append(target.data(), target.size());
    wire += " HTTP/1.1\r\nHost: localhost\r\n";
    if (!connection.empty()) {
        wire += "Connection: ";
        wire.append(connection.data(), connection.size());
        wire += "\r\n";
    }
    wire += "\r\n";
    return wire;
}

[[nodiscard]] std::string response_body(const std::string& response) {
    const std::size_t split = response.find("\r\n\r\n");
    return split == std::string::npos ? std::string{} : response.substr(split + 4U);
}

}  // namespace

TEST(HttpConnectionGuardsAcceptanceTest, ClosesAnIdleConnectionWithoutSendingAResponse) {
    HttpServerConfig config;
    config.idle_timeout = 100ms;
    config.header_timeout = 2s;
    config.body_timeout = 2s;
    TestServer server(config);
    TestClient client(server.port());

    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, Returns408ForAnIncompleteHeader) {
    HttpServerConfig config;
    config.idle_timeout = 2s;
    config.header_timeout = 120ms;
    config.body_timeout = 2s;
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all("GET /hello HTTP/1.1\r\nHost: localhost\r\nX-Partial:"));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 408 Request Timeout\r\n"), std::string::npos);
    EXPECT_NE(response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "408 Request Timeout\n");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, Returns408ForAnIncompleteRequestBody) {
    HttpServerConfig config;
    config.idle_timeout = 2s;
    config.header_timeout = 2s;
    config.body_timeout = 120ms;
    TestServer server(config);
    TestClient client(server.port());

    const std::string request =
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nab";
    ASSERT_TRUE(client.send_all(request));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.1 408 Request Timeout\r\n"), std::string::npos);
    EXPECT_NE(response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "408 Request Timeout\n");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, HeaderDeadlineIsAbsoluteAndNotExtendedByTrickleBytes) {
    HttpServerConfig config;
    config.idle_timeout = 2s;
    config.header_timeout = 400ms;
    config.body_timeout = 2s;
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all("GET /hello HTTP/1.1\r\n"));
    std::this_thread::sleep_for(300ms);
    ASSERT_TRUE(client.send_all("Host: localhost"));
    const auto started = std::chrono::steady_clock::now();
    const std::string response = client.read_response();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(elapsed, 250ms);
    EXPECT_NE(response.find("HTTP/1.1 408 Request Timeout\r\n"), std::string::npos);
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, BodyDeadlineIsAbsoluteAndNotExtendedByTrickleBytes) {
    HttpServerConfig config;
    config.idle_timeout = 2s;
    config.header_timeout = 2s;
    config.body_timeout = 400ms;
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\na"));
    std::this_thread::sleep_for(300ms);
    ASSERT_TRUE(client.send_all("b"));
    const auto started = std::chrono::steady_clock::now();
    const std::string response = client.read_response();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(elapsed, 250ms);
    EXPECT_NE(response.find("HTTP/1.1 408 Request Timeout\r\n"), std::string::npos);
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, RefreshesIdleDeadlineAfterACompletedKeepAliveResponse) {
    HttpServerConfig config;
    config.idle_timeout = 300ms;
    config.header_timeout = 2s;
    config.body_timeout = 2s;
    TestServer server(config);
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello")));
    const std::string first = client.read_response();
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);

    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(client.send_all(make_request("/health", "close")));
    const std::string second = client.read_response();
    EXPECT_NE(second.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, ClosesAfterConfiguredRequestCount) {
    HttpServerConfig config;
    config.max_requests_per_connection = 2U;
    TestServer server(config);
    TestClient client(server.port());

    const std::string pipeline =
        make_request("/hello") + make_request("/health") + make_request("/hello");
    ASSERT_TRUE(client.send_all(pipeline));

    const std::string first = client.read_response();
    const std::string second = client.read_response();
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(first), "Hello from C++ HTTP Server!\n");
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, EnforcesPersistentConnectionQuotaAndReleasesItsSlot) {
    HttpServerConfig config;
    config.max_connections = 4U;
    config.max_persistent_connections = 1U;
    TestServer server(config);

    TestClient first(server.port());
    ASSERT_TRUE(first.send_all(make_request("/hello")));
    const std::string first_response = first.read_response();
    EXPECT_NE(first_response.find("Connection: keep-alive\r\n"), std::string::npos);

    TestClient second(server.port());
    ASSERT_TRUE(second.send_all(make_request("/health")));
    const std::string second_response = second.read_response();
    EXPECT_NE(second_response.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(second_response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_TRUE(second.read_until_eof().empty());

    ASSERT_TRUE(first.send_all(make_request("/health", "close")));
    const std::string closing_response = first.read_response();
    EXPECT_NE(closing_response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_TRUE(first.read_until_eof().empty());

    TestClient third(server.port());
    ASSERT_TRUE(third.send_all(make_request("/hello")));
    const std::string third_response = third.read_response();
    EXPECT_NE(third_response.find("Connection: keep-alive\r\n"), std::string::npos);
    ASSERT_TRUE(third.send_all(make_request("/health", "close")));
    (void)third.read_response();
    EXPECT_TRUE(third.read_until_eof().empty());
}

TEST(HttpConnectionGuardsAcceptanceTest, RejectsZeroTimeoutAndConnectionQuotaConfiguration) {
    HttpServerConfig config;
    config.idle_timeout = 0ms;
    EXPECT_THROW(HttpServer{config}, std::invalid_argument);

    config = HttpServerConfig{};
    config.header_timeout = 0ms;
    EXPECT_THROW(HttpServer{config}, std::invalid_argument);

    config = HttpServerConfig{};
    config.body_timeout = 0ms;
    EXPECT_THROW(HttpServer{config}, std::invalid_argument);

    config = HttpServerConfig{};
    config.max_requests_per_connection = 0U;
    EXPECT_THROW(HttpServer{config}, std::invalid_argument);

    config = HttpServerConfig{};
    config.max_persistent_connections = 0U;
    EXPECT_THROW(HttpServer{config}, std::invalid_argument);
}
