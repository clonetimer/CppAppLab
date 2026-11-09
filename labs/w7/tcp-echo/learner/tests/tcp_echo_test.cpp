#include "tcp_echo.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using cppapplab::net::TcpEchoServer;
using cppapplab::net::connect_tcp;
using cppapplab::net::send_all;

TEST(TcpEchoLearnerTest, ServerBindsAnEphemeralPort) {
    TcpEchoServer server(0U);
    EXPECT_NE(server.port(), 0U);
}

TEST(TcpEchoLearnerTest, EchoesOneClientPayload) {
    TcpEchoServer server(0U);
    auto finished = std::async(std::launch::async, [&] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    const std::string message = "hello tcp";
    ASSERT_TRUE(send_all(client, message.data(), message.size()));
    (void)::shutdown(client, SHUT_WR);
    std::string response;
    std::array<char, 64> buffer{};
    for (;;) {
        const ssize_t count = ::recv(client, buffer.data(), buffer.size(), 0);
        if (count <= 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(count));
    }
    ::close(client);
    EXPECT_EQ(response, message);
    EXPECT_EQ(finished.wait_for(1s), std::future_status::ready);
    finished.get();
}

TEST(TcpEchoLearnerTest, SendAllAcceptsAnEmptyPayload) {
    EXPECT_TRUE(send_all(-1, nullptr, 0U) == false);
}

TEST(TcpEchoLearnerTest, RejectsInvalidBacklog) {
    EXPECT_THROW(TcpEchoServer(0U, "127.0.0.1", 0), std::invalid_argument);
}

TEST(TcpEchoLearnerTest, RejectsInvalidIPv4Address) {
    EXPECT_THROW(TcpEchoServer(0U, "localhost"), std::invalid_argument);
}

TEST(TcpEchoLearnerTest, ClientRejectsInvalidIPv4Address) {
    EXPECT_THROW(connect_tcp("not-an-ip", 8080U), std::invalid_argument);
}

TEST(TcpEchoLearnerTest, ServerReportsBoundPort) {
    TcpEchoServer server(0U);
    EXPECT_GT(server.port(), 0U);
}

TEST(TcpEchoLearnerTest, EchoesBinaryPayload) {
    TcpEchoServer server(0U);
    auto finished = std::async(std::launch::async, [&] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    const std::string message("A\0B\nC", 5U);
    ASSERT_TRUE(send_all(client, message.data(), message.size()));
    (void)::shutdown(client, SHUT_WR);
    std::string response;
    char buffer[16]{};
    for (;;) {
        const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(client);
    EXPECT_EQ(response, message);
    finished.get();
}

TEST(TcpEchoLearnerTest, EchoesLargePayload) {
    TcpEchoServer server(0U);
    auto finished = std::async(std::launch::async, [&] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    const std::string message(128U * 1024U, 'x');
    ASSERT_TRUE(send_all(client, message.data(), message.size()));
    (void)::shutdown(client, SHUT_WR);
    std::string response;
    char buffer[8192]{};
    for (;;) {
        const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(client);
    EXPECT_EQ(response, message);
    finished.get();
}

TEST(TcpEchoLearnerTest, ServerClosesAcceptedClientOnPeerShutdown) {
    TcpEchoServer server(0U);
    auto finished = std::async(std::launch::async, [&] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    (void)::shutdown(client, SHUT_WR);
    char byte{};
    EXPECT_EQ(::recv(client, &byte, 1U, 0), 0);
    ::close(client);
    EXPECT_EQ(finished.wait_for(1s), std::future_status::ready);
    finished.get();
}
