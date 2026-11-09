#include "static_file_handler.hpp"

#include <iostream>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " handle <METHOD> <target>\n"
              << "Serve one request from the local www/ document root.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }
    if (argc != 4 || std::string(argv[1]) != "handle") {
        print_usage(argv[0]);
        return 2;
    }

    cppapplab::http::HttpRequest request;
    request.method = argv[2];
    request.target = argv[3];

    const cppapplab::http::StaticFileHandler handler("www");
    const cppapplab::http::HttpResponse response = handler.handle(request);
    std::cout << cppapplab::http::serialize_response(response);
    return 0;
}
