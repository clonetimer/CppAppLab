#include "windows_state_io.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        return 2;
    }
    try {
        const auto worker = std::stoul(argv[2]);
        for (unsigned i = 0; i < 20U; ++i) {
            const std::string row = std::to_string(worker) + ":" + std::to_string(i) + "\n";
            cppapplab::windows_state_io::append(std::filesystem::path(argv[1]), row, 1024U * 1024U);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
