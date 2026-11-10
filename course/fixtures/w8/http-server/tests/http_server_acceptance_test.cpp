#include "http_server.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using namespace cppapplab::http;

namespace {
class TemporaryRoot {
   public:
    TemporaryRoot() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("cppapplab-http-server-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryRoot() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& path() const noexcept { return path_; }
    void write(const std::string& name, const std::string& bytes) const {
        std::ofstream output(path_ / name, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create test document");
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!output) throw std::runtime_error("cannot write test document");
    }
   private:
    std::filesystem::path path_;
};

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
    std::uint16_t port() const noexcept { return server_.port(); }
   private:
    HttpServer server_;
    std::thread runner_;
};

class TestClient {
   public:
    explicit TestClient(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) throw std::runtime_error("socket failed");
        timeval timeout{};
        timeout.tv_sec = 4;
        (void)::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("connect failed");
        }
    }
    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;
    ~TestClient() { if (fd_ >= 0) (void)::close(fd_); }
    bool send_all(const std::string& bytes) const {
        std::size_t offset = 0U;
        while (offset < bytes.size()) {
            const ssize_t sent =
                ::send(fd_, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL);
            if (sent > 0) offset += static_cast<std::size_t>(sent);
            else if (sent < 0 && errno == EINTR) continue;
            else return false;
        }
        return true;
    }
    void shutdown_write() const { (void)::shutdown(fd_, SHUT_WR); }
    std::string read_all() const {
        std::string result;
        char buffer[8192];
        for (;;) {
            const ssize_t received = ::recv(fd_, buffer, sizeof(buffer), 0);
            if (received > 0) result.append(buffer, static_cast<std::size_t>(received));
            else if (received < 0 && errno == EINTR) continue;
            else if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                throw std::runtime_error("timed out waiting for HTTP response");
            else break;
        }
        return result;
    }
    void close_now() {
        if (fd_ >= 0) {
            (void)::close(fd_);
            fd_ = -1;
        }
    }
   private:
    int fd_{-1};
};

std::string request(const std::string& method, const std::string& target,
                    const std::string& body = {}) {
    std::string wire = method + " " + target + " HTTP/1.1\r\nHost: localhost\r\n";
    if (!body.empty()) wire += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    wire += "\r\n";
    wire += body;
    return wire;
}
std::string exchange(std::uint16_t port, const std::string& wire) {
    TestClient client(port);
    if (!client.send_all(wire)) throw std::runtime_error("send failed");
    return client.read_all();
}
std::string body_of(const std::string& response) {
    const auto split = response.find("\r\n\r\n");
    return split == std::string::npos ? std::string{} : response.substr(split + 4U);
}
}  // namespace

TEST(HttpServerAcceptanceTest, BindsKernelAssignedPort) {
    TestServer server;
    EXPECT_NE(server.port(), 0U);
}
TEST(HttpServerAcceptanceTest, ServesHelloThroughRealTcpPipeline) {
    TestServer server;
    const auto response = exchange(server.port(), request("GET", "/hello"));
    EXPECT_NE(response.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
    EXPECT_EQ(body_of(response), "Hello from C++ HTTP Server!\n");
}
TEST(HttpServerAcceptanceTest, EchoesBinaryBodyByteForByte) {
    TestServer server;
    std::string payload(5U, 'x');
    payload[1] = '\0';
    payload[3] = static_cast<char>(0xff);
    const auto response = exchange(server.port(), request("POST", "/echo", payload));
    EXPECT_NE(response.find("Content-Length: 5\r\n"), std::string::npos);
    EXPECT_EQ(body_of(response), payload);
}
TEST(HttpServerAcceptanceTest, ServesHealthAsJson) {
    TestServer server;
    const auto response = exchange(server.port(), request("GET", "/health"));
    EXPECT_NE(response.find("Content-Type: application/json\r\n"), std::string::npos);
    EXPECT_EQ(body_of(response), "{\"status\":\"ok\"}");
}
TEST(HttpServerAcceptanceTest, ServesStaticIndexFallback) {
    TemporaryRoot root;
    root.write("index.html", "<h1>native</h1>");
    HttpServerConfig config;
    config.document_root = root.path();
    TestServer server(config);
    EXPECT_EQ(body_of(exchange(server.port(), request("GET", "/"))), "<h1>native</h1>");
}
TEST(HttpServerAcceptanceTest, Returns404ForMissingStaticFile) {
    TestServer server;
    EXPECT_NE(exchange(server.port(), request("GET", "/absent.html"))
                  .find("HTTP/1.1 404 Not Found\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, Returns405ForKnownDynamicPathWrongMethod) {
    TestServer server;
    const auto response = exchange(server.port(), request("GET", "/echo"));
    EXPECT_NE(response.find("HTTP/1.1 405 Method Not Allowed\r\n"), std::string::npos);
    EXPECT_NE(response.find("Allow: POST\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, RejectsWhitespaceBeforeHeaderColon) {
    TestServer server;
    const auto response =
        exchange(server.port(), "GET / HTTP/1.1\r\nHost : localhost\r\n\r\n");
    EXPECT_NE(response.find("HTTP/1.1 400 Bad Request\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, Returns413ForRequestBufferLimit) {
    HttpServerConfig config;
    config.max_request_bytes = 128U;
    TestServer server(config);
    const auto response = exchange(server.port(), "GET /" + std::string(200U, 'a'));
    EXPECT_NE(response.find("HTTP/1.1 413 Content Too Large\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, ConvertsHandlerExceptionTo500) {
    Router router = make_default_router();
    router.add_route("GET", "/explode", [](const HttpRequest&) -> HttpResponse {
        throw std::runtime_error("expected handler failure");
    });
    TestServer server({}, std::move(router));
    EXPECT_NE(exchange(server.port(), request("GET", "/explode"))
                  .find("HTTP/1.1 500 Internal Server Error\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, HandlesFragmentedRequestLineAndHeaders) {
    TestServer server;
    TestClient client(server.port());
    ASSERT_TRUE(client.send_all("GET /hel"));
    std::this_thread::sleep_for(15ms);
    ASSERT_TRUE(client.send_all("lo HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    EXPECT_EQ(body_of(client.read_all()), "Hello from C++ HTTP Server!\n");
}
TEST(HttpServerAcceptanceTest, ResumesPartialWritesForLargeResponse) {
    Router router = make_default_router();
    const std::string payload(768U * 1024U, 'x');
    router.add_route("GET", "/large", [payload](const HttpRequest&) {
        HttpResponse response;
        response.body = payload;
        response.headers.emplace("Content-Type", "application/octet-stream");
        return response;
    });
    HttpServerConfig config;
    config.max_response_bytes = 1024U * 1024U;
    TestServer server(config, std::move(router));
    EXPECT_EQ(body_of(exchange(server.port(), request("GET", "/large"))).size(),
              payload.size());
}
TEST(HttpServerAcceptanceTest, RejectsConnectionsAboveConfiguredLimit) {
    HttpServerConfig config;
    config.max_connections = 1U;
    config.task_queue_capacity = 1U;
    TestServer server(config);
    TestClient first(server.port());
    TestClient second(server.port());
    EXPECT_NE(second.read_all().find("HTTP/1.1 503 Service Unavailable\r\n"),
              std::string::npos);
}
TEST(HttpServerAcceptanceTest, HandlesConcurrentClients) {
    TestServer server;
    std::vector<std::thread> clients;
    std::atomic<int> passed{0};
    for (int index = 0; index < 8; ++index) {
        clients.emplace_back([&] {
            try {
                if (body_of(exchange(server.port(), request("GET", "/health"))) ==
                    "{\"status\":\"ok\"}") {
                    passed.fetch_add(1);
                }
            } catch (...) {
            }
        });
    }
    for (auto& client : clients) client.join();
    EXPECT_EQ(passed.load(), 8);
}
TEST(HttpServerAcceptanceTest, Returns400ForIncompleteRequestAtEof) {
    TestServer server;
    TestClient client(server.port());
    ASSERT_TRUE(client.send_all("GET /hello"));
    client.shutdown_write();
    EXPECT_NE(client.read_all().find("HTTP/1.1 400 Bad Request\r\n"), std::string::npos);
}
TEST(HttpServerAcceptanceTest, RejectsStaleWorkerCompletionAfterDisconnect) {
    std::atomic<bool> entered{false};
    Router router = make_default_router();
    router.add_route("GET", "/slow", [&entered](const HttpRequest&) {
        entered.store(true);
        std::this_thread::sleep_for(120ms);
        HttpResponse response;
        response.body = "stale";
        return response;
    });
    TestServer server({}, std::move(router));
    {
        TestClient old_client(server.port());
        ASSERT_TRUE(old_client.send_all(request("GET", "/slow")));
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (!entered.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        ASSERT_TRUE(entered.load());
        old_client.close_now();
    }
    EXPECT_EQ(body_of(exchange(server.port(), request("GET", "/hello"))),
              "Hello from C++ HTTP Server!\n");
}
TEST(HttpServerAcceptanceTest, EventFdStopsBlockedEventLoop) {
    HttpServer server;
    auto finished = std::async(std::launch::async, [&server] { server.run(); });
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!server.running() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    ASSERT_TRUE(server.running());
    server.request_stop();
    EXPECT_EQ(finished.wait_for(1s), std::future_status::ready);
    finished.get();
    EXPECT_FALSE(server.running());
}
