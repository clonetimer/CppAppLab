#include "tcp_echo.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <utility>

namespace cppapplab::net {
namespace {

class UniqueFd {
   public:
    explicit UniqueFd(int value = -1) noexcept : value_(value) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    ~UniqueFd() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
    [[nodiscard]] int get() const noexcept {
        return value_;
    }
    [[nodiscard]] int release() noexcept {
        const int value = value_;
        value_ = -1;
        return value;
    }

   private:
    int value_;
};

[[nodiscard]] sockaddr_in make_address(const std::string& address, std::uint16_t port) {
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &result.sin_addr) != 1) {
        throw std::invalid_argument("bind/connect address must be a valid IPv4 literal");
    }
    return result;
}

[[noreturn]] void throw_system_error(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}

}  // namespace

TcpEchoServer::TcpEchoServer(std::uint16_t port, std::string bind_address, int backlog) {
    if (backlog <= 0) {
        throw std::invalid_argument("listen backlog must be positive");
    }

    UniqueFd listener(::socket(AF_INET, SOCK_STREAM, 0));
    if (listener.get() < 0) {
        throw_system_error("socket");
    }

    const int reuse = 1;
    if (::setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throw_system_error("setsockopt(SO_REUSEADDR)");
    }

    const sockaddr_in address = make_address(bind_address, port);
    if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throw_system_error("bind");
    }
    if (::listen(listener.get(), backlog) < 0) {
        throw_system_error("listen");
    }

    sockaddr_in bound{};
    socklen_t length = sizeof(bound);
    if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound), &length) < 0) {
        throw_system_error("getsockname");
    }

    port_ = ntohs(bound.sin_port);
    listen_fd_ = listener.release();
}

TcpEchoServer::~TcpEchoServer() {
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
    }
}

std::uint16_t TcpEchoServer::port() const noexcept {
    return port_;
}

void TcpEchoServer::serve_one() {
    int client_fd = -1;
    do {
        client_fd = ::accept(listen_fd_, nullptr, nullptr);
    } while (client_fd < 0 && errno == EINTR);
    if (client_fd < 0) {
        throw_system_error("accept");
    }

    UniqueFd client(client_fd);
    char buffer[4096];
    for (;;) {
        const ssize_t received = ::recv(client.get(), buffer, sizeof(buffer), 0);
        if (received == 0) {
            return;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_system_error("recv");
        }
        if (!send_all(client.get(), buffer, static_cast<std::size_t>(received))) {
            return;
        }
    }
}

int connect_tcp(const std::string& address, std::uint16_t port) {
    UniqueFd socket_fd(::socket(AF_INET, SOCK_STREAM, 0));
    if (socket_fd.get() < 0) {
        throw_system_error("socket");
    }
    const sockaddr_in endpoint = make_address(address, port);
    int result = -1;
    do {
        result = ::connect(socket_fd.get(), reinterpret_cast<const sockaddr*>(&endpoint),
                           sizeof(endpoint));
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        throw_system_error("connect");
    }
    return socket_fd.release();
}

bool send_all(int file_descriptor, const char* data, std::size_t size) {
    if (file_descriptor < 0 || (data == nullptr && size != 0U)) {
        errno = EINVAL;
        return false;
    }

    std::size_t offset = 0;
    while (offset < size) {
        const ssize_t sent = ::send(file_descriptor, data + offset, size - offset, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

}  // namespace cppapplab::net
