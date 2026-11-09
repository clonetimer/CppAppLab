#include "tcp_chat.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <unistd.h>

using namespace std::chrono_literals;

namespace net = cppapplab::net;

namespace {

class TestServer {
   public:
    explicit TestServer(net::ChatServerConfig config = {}) : server_(std::move(config)) {
        runner_ = std::thread([this] { server_.run(); });
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (!server_.running() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        if (!server_.running()) {
            server_.request_stop();
            runner_.join();
            throw std::runtime_error("server did not start");
        }
    }
    ~TestServer() {
        if (runner_.joinable()) {
            server_.request_stop();
            runner_.join();
        }
    }
    [[nodiscard]] std::uint16_t port() const noexcept { return server_.port(); }

   private:
    net::ChatServer server_;
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
        timeout.tv_sec = 2;
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
    ~TestClient() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    [[nodiscard]] bool send_text(const std::string& text) const {
        std::size_t offset = 0U;
        while (offset < text.size()) {
            const ssize_t count = ::send(fd_, text.data() + offset, text.size() - offset,
                                         MSG_NOSIGNAL);
            if (count > 0) {
                offset += static_cast<std::size_t>(count);
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else {
                return false;
            }
        }
        return true;
    }
    [[nodiscard]] std::optional<std::string> read_line() {
        for (;;) {
            const auto newline = input_.find('\n');
            if (newline != std::string::npos) {
                auto line = input_.substr(0U, newline + 1U);
                input_.erase(0U, newline + 1U);
                return line;
            }
            char bytes[512];
            const ssize_t count = ::recv(fd_, bytes, sizeof(bytes), 0);
            if (count <= 0) {
                return std::nullopt;
            }
            input_.append(bytes, static_cast<std::size_t>(count));
        }
    }

   private:
    int fd_{-1};
    std::string input_;
};

void register_user(TestClient& client, const std::string& nickname) {
    ASSERT_TRUE(client.send_text("NICK " + nickname + "\n"));
    EXPECT_EQ(client.read_line(), "OK NICK " + nickname + "\n");
}

}  // namespace

TEST(TcpChatLearnerTest, BindsAnEphemeralPort) {
    TestServer server;
    EXPECT_NE(server.port(), 0U);
}

TEST(TcpChatLearnerTest, RejectsInvalidBacklog) {
    net::ChatServerConfig config;
    config.backlog = 0;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}

TEST(TcpChatLearnerTest, RejectsInvalidClientLimit) {
    net::ChatServerConfig config;
    config.max_clients = 0U;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}

TEST(TcpChatLearnerTest, RegistersNickname) {
    TestServer server;
    TestClient alice(server.port());
    register_user(alice, "alice");
}

TEST(TcpChatLearnerTest, RejectsDuplicateNickname) {
    TestServer server;
    TestClient alice(server.port());
    TestClient bob(server.port());
    register_user(alice, "alice");
    ASSERT_TRUE(bob.send_text("NICK alice\n"));
    EXPECT_EQ(bob.read_line(), "ERR nickname unavailable\n");
}

TEST(TcpChatLearnerTest, BroadcastsMessagesToOtherClients) {
    TestServer server;
    TestClient alice(server.port());
    TestClient bob(server.port());
    register_user(alice, "alice");
    register_user(bob, "bob");
    EXPECT_EQ(alice.read_line(), "* bob joined\n");
    ASSERT_TRUE(alice.send_text("MSG hello\n"));
    EXPECT_EQ(bob.read_line(), "alice: hello\n");
}

TEST(TcpChatLearnerTest, HandlesFragmentedCommands) {
    TestServer server;
    TestClient alice(server.port());
    ASSERT_TRUE(alice.send_text("NI"));
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(alice.send_text("CK alice\n"));
    EXPECT_EQ(alice.read_line(), "OK NICK alice\n");
}

TEST(TcpChatLearnerTest, RejectsMessageBeforeRegistration) {
    TestServer server;
    TestClient alice(server.port());
    ASSERT_TRUE(alice.send_text("MSG hello\n"));
    EXPECT_EQ(alice.read_line(), "ERR register first\n");
}

TEST(TcpChatLearnerTest, DisconnectsOnQuit) {
    TestServer server;
    TestClient alice(server.port());
    TestClient bob(server.port());
    register_user(alice, "alice");
    register_user(bob, "bob");
    EXPECT_EQ(alice.read_line(), "* bob joined\n");
    ASSERT_TRUE(alice.send_text("QUIT\n"));
    EXPECT_EQ(bob.read_line(), "* alice left\n");
    EXPECT_EQ(alice.read_line(), std::nullopt);
}

TEST(TcpChatLearnerTest, StopRequestWakesEventLoop) {
    net::ChatServer server;
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
