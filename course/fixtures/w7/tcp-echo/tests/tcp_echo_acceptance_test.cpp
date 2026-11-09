#include <algorithm>
#include "tcp_echo.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;
using cppapplab::net::TcpEchoServer;
using cppapplab::net::connect_tcp;
using cppapplab::net::send_all;

namespace {

std::string echo_round_trip(const std::string& payload, std::size_t chunk_size = 0U) {
    TcpEchoServer server(0U);
    auto server_done = std::async(std::launch::async, [&server] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());

    if (chunk_size == 0U) {
        EXPECT_TRUE(send_all(client, payload.data(), payload.size()));
    } else {
        for (std::size_t offset = 0; offset < payload.size(); offset += chunk_size) {
            const std::size_t count = std::min(chunk_size, payload.size() - offset);
            EXPECT_TRUE(send_all(client, payload.data() + offset, count));
        }
    }
    EXPECT_EQ(::shutdown(client, SHUT_WR), 0);

    std::string response;
    std::array<char, 8192> buffer{};
    for (;;) {
        const ssize_t received = ::recv(client, buffer.data(), buffer.size(), 0);
        if (received == 0) {
            break;
        }
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received < 0) {
            ADD_FAILURE() << "recv failed";
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(received));
    }
    ::close(client);
    EXPECT_EQ(server_done.wait_for(2s), std::future_status::ready);
    server_done.get();
    return response;
}

}  // namespace

TEST(TcpEchoAcceptanceTest, BindsEphemeralPort) {
    TcpEchoServer server(0U);
    EXPECT_NE(server.port(), 0U);
}

TEST(TcpEchoAcceptanceTest, RejectsZeroBacklog) {
    EXPECT_THROW(TcpEchoServer(0U, "127.0.0.1", 0), std::invalid_argument);
}

TEST(TcpEchoAcceptanceTest, RejectsNegativeBacklog) {
    EXPECT_THROW(TcpEchoServer(0U, "127.0.0.1", -1), std::invalid_argument);
}

TEST(TcpEchoAcceptanceTest, RejectsMalformedBindAddress) {
    EXPECT_THROW(TcpEchoServer(0U, "localhost"), std::invalid_argument);
}

TEST(TcpEchoAcceptanceTest, RejectsMalformedClientAddress) {
    EXPECT_THROW(connect_tcp("not-an-ip", 8080U), std::invalid_argument);
}

TEST(TcpEchoAcceptanceTest, RejectsNegativeSendDescriptor) {
    const char value = 'x';
    EXPECT_FALSE(send_all(-1, &value, 1U));
}

TEST(TcpEchoAcceptanceTest, RejectsNullNonEmptySendBuffer) {
    EXPECT_FALSE(send_all(1, nullptr, 1U));
}

TEST(TcpEchoAcceptanceTest, AcceptsEmptySendOnValidSocket) {
    int descriptors[2]{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors), 0);
    EXPECT_TRUE(send_all(descriptors[0], nullptr, 0U));
    ::close(descriptors[0]);
    ::close(descriptors[1]);
}

TEST(TcpEchoAcceptanceTest, EchoesPlainText) {
    EXPECT_EQ(echo_round_trip("hello over TCP"), "hello over TCP");
}

TEST(TcpEchoAcceptanceTest, EchoesBinaryBytes) {
    const std::string payload("A\0B\r\nC\xff", 7U);
    EXPECT_EQ(echo_round_trip(payload), payload);
}

TEST(TcpEchoAcceptanceTest, EchoesPayloadLargerThanReceiveBuffer) {
    const std::string payload(256U * 1024U, 'q');
    EXPECT_EQ(echo_round_trip(payload), payload);
}

TEST(TcpEchoAcceptanceTest, PreservesOneByteFragmentation) {
    EXPECT_EQ(echo_round_trip("fragmented TCP stream", 1U), "fragmented TCP stream");
}

TEST(TcpEchoAcceptanceTest, PreservesMediumFragments) {
    const std::string payload(8193U, 'm');
    EXPECT_EQ(echo_round_trip(payload, 37U), payload);
}

TEST(TcpEchoAcceptanceTest, HandlesEmptyPeerPayload) {
    EXPECT_TRUE(echo_round_trip("").empty());
}

TEST(TcpEchoAcceptanceTest, HandlesPeerWriteShutdown) {
    EXPECT_EQ(echo_round_trip("before FIN"), "before FIN");
}

TEST(TcpEchoAcceptanceTest, UsesLoopbackByDefault) {
    TcpEchoServer server(0U);
    auto done = std::async(std::launch::async, [&server] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    const char message[] = "x";
    ASSERT_TRUE(send_all(client, message, 1U));
    (void)::shutdown(client, SHUT_WR);
    char echoed{};
    EXPECT_EQ(::recv(client, &echoed, 1U, 0), 1);
    EXPECT_EQ(echoed, 'x');
    ::close(client);
    done.get();
}

TEST(TcpEchoAcceptanceTest, RejectsSecondListenerOnSameEndpoint) {
    TcpEchoServer first(0U);
    EXPECT_THROW(TcpEchoServer(first.port()), std::runtime_error);
}

TEST(TcpEchoAcceptanceTest, SendAllWritesLargeSocketPairPayload) {
    int descriptors[2]{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors), 0);
    const std::string expected(512U * 1024U, 'z');
    auto reader = std::async(std::launch::async, [&] {
        std::string actual;
        std::array<char, 4096> buffer{};
        for (;;) {
            const ssize_t count = ::read(descriptors[1], buffer.data(), buffer.size());
            if (count <= 0) {
                break;
            }
            actual.append(buffer.data(), static_cast<std::size_t>(count));
        }
        return actual;
    });
    EXPECT_TRUE(send_all(descriptors[0], expected.data(), expected.size()));
    ::shutdown(descriptors[0], SHUT_WR);
    ::close(descriptors[0]);
    EXPECT_EQ(reader.get(), expected);
    ::close(descriptors[1]);
}

TEST(TcpEchoAcceptanceTest, EchoesPayloadWithEmbeddedNewlines) {
    const std::string payload = "first\nsecond\nthird\n";
    EXPECT_EQ(echo_round_trip(payload), payload);
}

TEST(TcpEchoAcceptanceTest, EchoesPayloadWithNoTerminator) {
    EXPECT_EQ(echo_round_trip("no newline at end"), "no newline at end");
}

TEST(TcpEchoAcceptanceTest, ServerClosesAcceptedSocketAfterClientFin) {
    TcpEchoServer server(0U);
    auto done = std::async(std::launch::async, [&server] { server.serve_one(); });
    const int client = connect_tcp("127.0.0.1", server.port());
    (void)::shutdown(client, SHUT_WR);
    char byte{};
    EXPECT_EQ(::recv(client, &byte, 1U, 0), 0);
    ::close(client);
    EXPECT_EQ(done.wait_for(2s), std::future_status::ready);
    done.get();
}
