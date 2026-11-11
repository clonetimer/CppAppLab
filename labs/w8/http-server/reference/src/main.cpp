#include "http_server.hpp"

#include <cerrno>
#include <cstdlib>
#include <exception>
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

void usage(const char* program) {
    std::cout << "Usage: " << program << " server <port> [document-root]\n"
              << "Runs a single-Reactor HTTP/1.1 server; one request per TCP connection.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage(argv[0]);
        return 0;
    }
    if ((argc != 3 && argc != 4) || std::string(argv[1]) != "server") {
        usage(argv[0]);
        return 2;
    }

    std::uint16_t port = 0U;
    if (!parse_port(argv[2], port)) {
        std::cerr << "invalid port\n";
        return 2;
    }

    try {
        cppapplab::http::HttpServerConfig config;
        config.port = port;
        if (argc == 4) {
            config.document_root = argv[3];
        }
        cppapplab::http::HttpServer server(std::move(config));
        std::cout << "listening on " << server.port() << '\n' << std::flush;
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
