#include "cppapplab/app.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::cout << cppapplab::usage();
        return 0;
    }

    const std::string_view command{argv[1]};

    if (command == "--help" || command == "-h") {
        std::cout << cppapplab::usage();
        return 0;
    }

    if (command == "--version" || command == "-v") {
        std::cout << cppapplab::version() << '\n';
        return 0;
    }

    std::cerr << "Unknown argument: " << command << "\n\n";
    std::cerr << cppapplab::usage();
    return 2;
}
