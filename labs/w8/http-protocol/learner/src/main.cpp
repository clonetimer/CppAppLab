#include "http_message.hpp"

#include <iostream>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << " parse-line \"METHOD target HTTP/1.1\"\n"
              << "  " << program << " response\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (argc == 3 && std::string(argv[1]) == "parse-line") {
        cppapplab::http::HttpRequest request;
        if (!cppapplab::http::parse_request_line(argv[2], request)) {
            std::cerr << "400 Bad Request: invalid request line\n";
            return 1;
        }
        std::cout << "method=" << request.method << '\n'
                  << "target=" << request.target << '\n'
                  << "version=" << request.version << '\n';
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "response") {
        cppapplab::http::HttpResponse response;
        response.headers.emplace("Content-Type", "text/plain");
        response.body = "Hello, world!";
        std::cout << cppapplab::http::serialize_response(response);
        return 0;
    }

    print_usage(argv[0]);
    return 2;
}
