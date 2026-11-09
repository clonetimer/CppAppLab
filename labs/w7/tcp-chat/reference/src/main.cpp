#include "tcp_chat.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

bool parse_port(const char* text, std::uint16_t& port) {
    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 65535UL) {
        return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " server <port>\n";
        return 0;
    }
    if (argc != 3 || std::string(argv[1]) != "server") {
        std::cerr << "Usage: " << argv[0] << " server <port>\n";
        return 2;
    }

    std::uint16_t port = 0U;
    if (!parse_port(argv[2], port)) {
        std::cerr << "invalid port\n";
        return 2;
    }

    try {
        cppapplab::net::ChatServerConfig config;
        config.port = port;
        cppapplab::net::ChatServer server(std::move(config));
        std::cout << "listening on 127.0.0.1:" << server.port() << '\n';
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
