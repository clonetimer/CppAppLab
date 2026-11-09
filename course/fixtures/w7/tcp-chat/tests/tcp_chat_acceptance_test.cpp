#include "tcp_chat.hpp"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

using namespace std::chrono_literals;

namespace net = cppapplab::net;

namespace {

class RunningServer {
   public:
    explicit RunningServer(net::ChatServerConfig config = {}) : server_(std::move(config)) {
        runner_ = std::thread([this] {
            try {
                server_.run();
            } catch (...) {
                error_ = std::current_exception();
            }
        });
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (!server_.running() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        if (!server_.running()) {
            stop_and_join();
            throw std::runtime_error("chat server did not enter its event loop");
        }
    }
    ~RunningServer() {
        if (runner_.joinable()) {
            server_.request_stop();
            runner_.join();
        }
    }
    [[nodiscard]] std::uint16_t port() const noexcept { return server_.port(); }

   private:
    void stop_and_join() {
        if (runner_.joinable()) {
            server_.request_stop();
            runner_.join();
        }
        if (error_) {
            std::rethrow_exception(error_);
        }
    }
    net::ChatServer server_;
    std::thread runner_;
    std::exception_ptr error_;
};

class Client {
   public:
    explicit Client(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) {
            throw std::runtime_error("client socket failed");
        }
        timeval timeout{};
        timeout.tv_sec = 2;
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("setsockopt timeout failed");
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("client connect failed");
        }
    }
    ~Client() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]] bool send_text(const std::string& text) const {
        std::size_t offset = 0U;
        while (offset < text.size()) {
            const ssize_t sent =
                ::send(fd_, text.data() + offset, text.size() - offset, MSG_NOSIGNAL);
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
    [[nodiscard]] std::optional<std::string> read_line() {
        for (;;) {
            const std::size_t newline = buffered_.find('\n');
            if (newline != std::string::npos) {
                std::string line = buffered_.substr(0U, newline + 1U);
                buffered_.erase(0U, newline + 1U);
                return line;
            }
            char chunk[1024];
            const ssize_t received = ::recv(fd_, chunk, sizeof(chunk), 0);
            if (received == 0) {
                return std::nullopt;
            }
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return std::nullopt;
            }
            buffered_.append(chunk, static_cast<std::size_t>(received));
        }
    }
    [[nodiscard]] std::optional<std::string> try_read_line() {
        timeval timeout{};
        timeout.tv_usec = 150000;
        (void)::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        return read_line();
    }

   private:
    int fd_{-1};
    std::string buffered_;
};

void register_nickname(Client& client, const std::string& nickname) {
    ASSERT_TRUE(client.send_text("NICK " + nickname + "\n"));
    EXPECT_EQ(client.read_line(), "OK NICK " + nickname + "\n");
}

}  // namespace

TEST(TcpChatAcceptanceTest, BindsAnEphemeralPort) {
    RunningServer server;
    EXPECT_NE(server.port(), 0U);
}
TEST(TcpChatAcceptanceTest, RejectsZeroBacklog) {
    net::ChatServerConfig config;
    config.backlog = 0;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}
TEST(TcpChatAcceptanceTest, RejectsZeroClientLimit) {
    net::ChatServerConfig config;
    config.max_clients = 0U;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}
TEST(TcpChatAcceptanceTest, RejectsZeroLineLimit) {
    net::ChatServerConfig config;
    config.max_line_bytes = 0U;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}
TEST(TcpChatAcceptanceTest, RejectsZeroPendingOutputLimit) {
    net::ChatServerConfig config;
    config.max_pending_bytes = 0U;
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}
TEST(TcpChatAcceptanceTest, RejectsMalformedBindAddress) {
    net::ChatServerConfig config;
    config.bind_address = "localhost";
    EXPECT_THROW(net::ChatServer(std::move(config)), std::invalid_argument);
}
TEST(TcpChatAcceptanceTest, RegistersAValidNickname) {
    RunningServer server;
    Client alice(server.port());
    register_nickname(alice, "alice_01");
}
TEST(TcpChatAcceptanceTest, RejectsInvalidNicknameCharacters) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("NICK alice!\n"));
    EXPECT_EQ(alice.read_line(), "ERR invalid nickname\n");
}
TEST(TcpChatAcceptanceTest, RejectsNicknameLongerThanTwentyFourBytes) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("NICK abcdefghijklmnopqrstuvwxy\n"));
    EXPECT_EQ(alice.read_line(), "ERR invalid nickname\n");
}
TEST(TcpChatAcceptanceTest, AcceptsTwentyFourByteNickname) {
    RunningServer server;
    Client alice(server.port());
    register_nickname(alice, "abcdefghijklmnopqrstuvwx");
}
TEST(TcpChatAcceptanceTest, RejectsDuplicateNickname) {
    RunningServer server;
    Client alice(server.port());
    Client bob(server.port());
    register_nickname(alice, "alice");
    ASSERT_TRUE(bob.send_text("NICK alice\n"));
    EXPECT_EQ(bob.read_line(), "ERR nickname unavailable\n");
}
TEST(TcpChatAcceptanceTest, RejectsSecondNicknameRegistration) {
    RunningServer server;
    Client alice(server.port());
    register_nickname(alice, "alice");
    ASSERT_TRUE(alice.send_text("NICK other\n"));
    EXPECT_EQ(alice.read_line(), "ERR already registered\n");
}
TEST(TcpChatAcceptanceTest, RequiresRegistrationBeforeMessages) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("MSG hello\n"));
    EXPECT_EQ(alice.read_line(), "ERR register first\n");
}
TEST(TcpChatAcceptanceTest, RejectsEmptyMessage) {
    RunningServer server;
    Client alice(server.port());
    register_nickname(alice, "alice");
    ASSERT_TRUE(alice.send_text("MSG \n"));
    EXPECT_EQ(alice.read_line(), "ERR empty message\n");
}
TEST(TcpChatAcceptanceTest, BroadcastsMessageToOtherRegisteredClients) {
    RunningServer server;
    Client alice(server.port());
    Client bob(server.port());
    register_nickname(alice, "alice");
    register_nickname(bob, "bob");
    EXPECT_EQ(alice.read_line(), "* bob joined\n");
    ASSERT_TRUE(alice.send_text("MSG hello everyone\n"));
    EXPECT_EQ(bob.read_line(), "alice: hello everyone\n");
}
TEST(TcpChatAcceptanceTest, DoesNotEchoMessageToSender) {
    RunningServer server;
    Client alice(server.port());
    Client bob(server.port());
    register_nickname(alice, "alice");
    register_nickname(bob, "bob");
    EXPECT_EQ(alice.read_line(), "* bob joined\n");
    ASSERT_TRUE(alice.send_text("MSG private-to-others\n"));
    EXPECT_EQ(bob.read_line(), "alice: private-to-others\n");
    EXPECT_EQ(alice.try_read_line(), std::nullopt);
}
TEST(TcpChatAcceptanceTest, HandlesFragmentedCommands) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("NI"));
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(alice.send_text("CK alice\n"));
    EXPECT_EQ(alice.read_line(), "OK NICK alice\n");
}
TEST(TcpChatAcceptanceTest, HandlesCoalescedCommands) {
    RunningServer server;
    Client bob(server.port());
    Client alice(server.port());
    register_nickname(bob, "bob");
    ASSERT_TRUE(alice.send_text("NICK alice\nMSG one-write\n"));
    EXPECT_EQ(alice.read_line(), "OK NICK alice\n");
    EXPECT_EQ(bob.read_line(), "* alice joined\n");
    EXPECT_EQ(bob.read_line(), "alice: one-write\n");
}
TEST(TcpChatAcceptanceTest, AcceptsCRLFCommandTerminators) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("NICK alice\r\n"));
    EXPECT_EQ(alice.read_line(), "OK NICK alice\n");
}
TEST(TcpChatAcceptanceTest, UnknownCommandReturnsAnError) {
    RunningServer server;
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("WHO\n"));
    EXPECT_EQ(alice.read_line(), "ERR unknown command\n");
}
TEST(TcpChatAcceptanceTest, QuitNotifiesOtherClients) {
    RunningServer server;
    Client alice(server.port());
    Client bob(server.port());
    register_nickname(alice, "alice");
    register_nickname(bob, "bob");
    EXPECT_EQ(alice.read_line(), "* bob joined\n");
    ASSERT_TRUE(alice.send_text("QUIT\n"));
    EXPECT_EQ(bob.read_line(), "* alice left\n");
    EXPECT_EQ(alice.read_line(), std::nullopt);
}
TEST(TcpChatAcceptanceTest, DisconnectsClientWithOverlongLine) {
    net::ChatServerConfig config;
    config.max_line_bytes = 8U;
    RunningServer server(config);
    Client alice(server.port());
    ASSERT_TRUE(alice.send_text("NICK abcdefghijklmnop\n"));
    EXPECT_EQ(alice.read_line(), std::nullopt);
}
TEST(TcpChatAcceptanceTest, EnforcesMaximumClientCount) {
    net::ChatServerConfig config;
    config.max_clients = 1U;
    RunningServer server(config);
    Client first(server.port());
    register_nickname(first, "first");
    Client second(server.port());
    EXPECT_EQ(second.read_line(), std::nullopt);
}
TEST(TcpChatAcceptanceTest, StopRequestWakesEpollWait) {
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
TEST(TcpChatAcceptanceTest, RunCanOnlyBeStartedOnce) {
    net::ChatServer server;
    auto finished = std::async(std::launch::async, [&server] { server.run(); });
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!server.running() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    server.request_stop();
    finished.get();
    EXPECT_THROW(server.run(), std::logic_error);
}
