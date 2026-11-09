#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cppapplab::net {

class TcpEchoServer {
   public:
    explicit TcpEchoServer(std::uint16_t port,
                           std::string bind_address = "127.0.0.1",
                           int backlog = 16);
    TcpEchoServer(const TcpEchoServer&) = delete;
    TcpEchoServer& operator=(const TcpEchoServer&) = delete;
    ~TcpEchoServer();

    [[nodiscard]] std::uint16_t port() const noexcept;
    void serve_one();

   private:
    int listen_fd_{-1};
    std::uint16_t port_{0};
};

[[nodiscard]] int connect_tcp(const std::string& address, std::uint16_t port);
[[nodiscard]] bool send_all(int file_descriptor, const char* data, std::size_t size);

}  // namespace cppapplab::net
