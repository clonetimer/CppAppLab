#include "process_runner.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: system-lab <program> [args...]\n";
        return 2;
    }

    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const auto result = cppapplab::system_lab::run_process(arguments);
    std::cout << result.output;
    return result.exit_code;
}
