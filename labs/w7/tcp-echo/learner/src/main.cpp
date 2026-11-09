#include "tcp_echo.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void print_usage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << " server <port>\n"
              << "  " << program << " client <port> <message>\n";
}

bool parse_port(const char* text, std::uint16_t& port) {
    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0UL || value > 65535UL) {
        return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    std::uint16_t port = 0;
    if (!parse_port(argv[2], port)) {
        std::cerr << "port must be in the range 1..65535\n";
        return 2;
    }

    try {
        if (std::string(argv[1]) == "server" && argc == 3) {
            cppapplab::net::TcpEchoServer server(port);
            std::cout << "listening on 127.0.0.1:" << server.port() << '\n';
            server.serve_one();
            return 0;
        }
        if (std::string(argv[1]) == "client" && argc == 4) {
            const int socket_fd = cppapplab::net::connect_tcp("127.0.0.1", port);
            const std::string message = argv[3];
            if (!cppapplab::net::send_all(socket_fd, message.data(), message.size())) {
                ::close(socket_fd);
                std::cerr << "send failed\n";
                return 1;
            }
            (void)::shutdown(socket_fd, SHUT_WR);

            char buffer[4096];
            for (;;) {
                const ssize_t received = ::recv(socket_fd, buffer, sizeof(buffer), 0);
                if (received == 0) {
                    break;
                }
                if (received < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    ::close(socket_fd);
                    std::cerr << "recv failed\n";
                    return 1;
                }
                std::cout.write(buffer, received);
            }
            ::close(socket_fd);
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    print_usage(argv[0]);
    return 2;
}
