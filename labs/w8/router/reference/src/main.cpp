#include "api_handlers.hpp"
#include "dispatcher.hpp"
#include "http_request_parser.hpp"

#include <iostream>
#include <string>

namespace {

void usage(const char* program) {
    std::cout << "Usage: " << program << " parse-stdin [document-root]\n"
              << "Read exactly one HTTP request from stdin; no listening socket is opened.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage(argv[0]);
        return 0;
    }
    if ((argc != 2 && argc != 3) || std::string(argv[1]) != "parse-stdin") {
        usage(argv[0]);
        return 2;
    }

    try {
        constexpr std::size_t max_input = 16U * 1024U + 1024U * 1024U;
        std::string input;
        char chunk[4096];
        while (std::cin.read(chunk, sizeof(chunk)) || std::cin.gcount() > 0) {
            const auto count = static_cast<std::size_t>(std::cin.gcount());
            if (count > max_input - input.size()) {
                std::cerr << "request exceeds the CLI input limit\n";
                return 2;
            }
            input.append(chunk, count);
        }
        if (std::cin.bad()) {
            std::cerr << "cannot read standard input\n";
            return 2;
        }

        const auto parsed = cppapplab::http::HttpRequestParser{}.parse(input);
        if (parsed.status != cppapplab::http::ParseStatus::Complete) {
            cppapplab::http::HttpResponse response;
            response.status_code = 400;
            response.reason_phrase = "Bad Request";
            response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
            response.body = "400 Bad Request\n";
            std::cout << cppapplab::http::serialize_response(response);
            std::cerr << "incomplete or malformed request: " << parsed.error << '\n';
            return 1;
        }
        if (parsed.consumed != input.size()) {
            std::cerr << "this CLI accepts exactly one HTTP request\n";
            return 2;
        }

        const auto router = cppapplab::http::make_default_router();
        const cppapplab::http::StaticFileHandler files(argc == 3 ? argv[2] : "www");
        const auto response = cppapplab::http::dispatch_request(parsed.request, router, files);
        std::cout << cppapplab::http::serialize_response(response);
        return std::cout.good() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
