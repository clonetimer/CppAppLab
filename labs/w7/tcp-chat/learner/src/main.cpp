#include "tcp_chat.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " server <port>\n";
        return 0;
    }
    if (argc != 3 || std::string(argv[1]) != "server") {
        std::cerr << "Usage: " << argv[0] << " server <port>\n";
        return 2;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || parsed > 65535UL) {
        std::cerr << "invalid port\n";
        return 2;
    }

    try {
        cppapplab::net::ChatServerConfig config;
        config.port = static_cast<std::uint16_t>(parsed);
        cppapplab::net::ChatServer server(std::move(config));
        std::cout << "listening on 127.0.0.1:" << server.port() << '\n';
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
