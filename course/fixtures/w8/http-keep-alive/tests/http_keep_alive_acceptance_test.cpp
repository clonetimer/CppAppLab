#include "http_server.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
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

    void shutdown_write() const {
        (void)::shutdown(fd_, SHUT_WR);
    }

    [[nodiscard]] std::string read_response() {
        std::size_t header_end = read_buffer_.find("\r\n\r\n");
        while (header_end == std::string::npos) {
            receive_more();
            header_end = read_buffer_.find("\r\n\r\n");
        }

        const std::string headers = read_buffer_.substr(0U, header_end);
        const std::string content_length_prefix = "Content-Length: ";
        const std::size_t length_position = headers.find(content_length_prefix);
        if (length_position == std::string::npos) {
            throw std::runtime_error("response is missing Content-Length");
        }
        const std::size_t value_begin = length_position + content_length_prefix.size();
        const std::size_t line_end = headers.find("\r\n", value_begin);
        const std::string length_text =
            headers.substr(value_begin, line_end == std::string::npos
                                            ? std::string::npos
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
                                       std::string_view version = "HTTP/1.1",
                                       std::string_view connection = {}) {
    std::string wire = "GET ";
    wire.append(target.data(), target.size());
    wire += " ";
    wire.append(version.data(), version.size());
    wire += "\r\nHost: localhost\r\n";
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

TEST(HttpKeepAliveAcceptanceTest, ReusesOneConnectionForSequentialRequests) {
    TestServer server;
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello")));
    const std::string first = client.read_response();
    EXPECT_NE(first.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_EQ(response_body(first), "Hello from C++ HTTP Server!\n");

    ASSERT_TRUE(client.send_all(make_request("/health", "HTTP/1.1", "close")));
    const std::string second = client.read_response();
    EXPECT_NE(second.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, ProcessesPipelinedRequestsInOrder) {
    TestServer server;
    TestClient client(server.port());

    const std::string pipeline =
        make_request("/hello") + make_request("/health", "HTTP/1.1", "close");
    ASSERT_TRUE(client.send_all(pipeline));

    const std::string first = client.read_response();
    const std::string second = client.read_response();
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_EQ(response_body(first), "Hello from C++ HTTP Server!\n");
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, ExplicitCloseDiscardsPipelinedRequestsAfterCurrentResponse) {
    TestServer server;
    TestClient client(server.port());

    const std::string pipeline =
        make_request("/hello", "HTTP/1.1", "close") + make_request("/health");
    ASSERT_TRUE(client.send_all(pipeline));

    const std::string response = client.read_response();
    EXPECT_NE(response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "Hello from C++ HTTP Server!\n");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, HandlesFragmentedSubsequentRequest) {
    TestServer server;
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello")));
    const std::string first = client.read_response();
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);

    ASSERT_TRUE(client.send_all("GET /heal HTTP/1.1\r\nHost: localhost\r\n"));
    std::this_thread::sleep_for(15ms);
    ASSERT_TRUE(client.send_all("Connection: close\r\n\r\n"));

    const std::string second = client.read_response();
    EXPECT_NE(second.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, Http10DefaultsToCloseAndUsesHttp10ResponseVersion) {
    TestServer server;
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello", "HTTP/1.0")));
    const std::string response = client.read_response();
    EXPECT_NE(response.find("HTTP/1.0 200 OK\r\n"), std::string::npos);
    EXPECT_NE(response.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(response), "Hello from C++ HTTP Server!\n");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, Http10KeepAliveRequiresExplicitOptIn) {
    TestServer server;
    TestClient client(server.port());

    ASSERT_TRUE(client.send_all(make_request("/hello", "HTTP/1.0", "keep-alive")));
    const std::string first = client.read_response();
    EXPECT_NE(first.find("HTTP/1.0 200 OK\r\n"), std::string::npos);
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);

    ASSERT_TRUE(client.send_all(make_request("/health", "HTTP/1.0", "close")));
    const std::string second = client.read_response();
    EXPECT_NE(second.find("HTTP/1.0 200 OK\r\n"), std::string::npos);
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, HalfClosedClientDrainsCompleteBufferedPipelineThenCloses) {
    TestServer server;
    TestClient client(server.port());

    const std::string pipeline = make_request("/hello") + make_request("/health");
    ASSERT_TRUE(client.send_all(pipeline));
    client.shutdown_write();

    const std::string first = client.read_response();
    const std::string second = client.read_response();
    EXPECT_NE(first.find("Connection: keep-alive\r\n"), std::string::npos);
    EXPECT_EQ(response_body(first), "Hello from C++ HTTP Server!\n");
    EXPECT_NE(second.find("Connection: close\r\n"), std::string::npos);
    EXPECT_EQ(response_body(second), "{\"status\":\"ok\"}");
    EXPECT_TRUE(client.read_until_eof().empty());
}

TEST(HttpKeepAliveAcceptanceTest, DoesNotDispatchNextRequestBeforeCurrentResponseIsReady) {
    std::mutex mutex;
    std::condition_variable condition;
    bool first_entered = false;
    bool release_first = false;
    bool second_entered = false;

    Router router = make_default_router();
    router.add_route("GET", "/first", [&](const HttpRequest&) {
        std::unique_lock<std::mutex> lock(mutex);
        first_entered = true;
        condition.notify_all();
        condition.wait(lock, [&] { return release_first; });
        HttpResponse response;
        response.body = "first";
        return response;
    });
    router.add_route("GET", "/second", [&](const HttpRequest&) {
        std::lock_guard<std::mutex> lock(mutex);
        second_entered = true;
        condition.notify_all();
        HttpResponse response;
        response.body = "second";
        return response;
    });

    TestServer server({}, std::move(router));
    TestClient client(server.port());
    const std::string pipeline =
        make_request("/first") + make_request("/second", "HTTP/1.1", "close");
    ASSERT_TRUE(client.send_all(pipeline));

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(condition.wait_for(lock, 1s, [&] { return first_entered; }));
    }
    std::this_thread::sleep_for(50ms);
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_FALSE(second_entered);
        release_first = true;
    }
    condition.notify_all();

    const std::string first = client.read_response();
    const std::string second = client.read_response();
    EXPECT_EQ(response_body(first), "first");
    EXPECT_EQ(response_body(second), "second");
    EXPECT_TRUE(client.read_until_eof().empty());
}
