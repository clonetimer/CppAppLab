#include "http_request_parser.hpp"

#include <iostream>
#include <iterator>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " parse-stdin\n"
              << "Read one HTTP/1.x request from standard input and report parser status.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }
    if (argc != 2 || std::string(argv[1]) != "parse-stdin") {
        print_usage(argv[0]);
        return 2;
    }

    const std::string input{std::istreambuf_iterator<char>(std::cin),
                             std::istreambuf_iterator<char>()};
    const cppapplab::http::ParseResult result =
        cppapplab::http::HttpRequestParser{}.parse(input);
    if (result.status == cppapplab::http::ParseStatus::NeedMoreData) {
        std::cout << "NeedMoreData\n";
        return 0;
    }
    if (result.status == cppapplab::http::ParseStatus::Error) {
        std::cout << "Error: " << result.error << '\n';
        return 1;
    }

    std::cout << "Complete\n"
              << "method=" << result.request.method << '\n'
              << "target=" << result.request.target << '\n'
              << "consumed=" << result.consumed << '\n';
    return 0;
}
