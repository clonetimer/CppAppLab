#include "cppapplab/learning.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <utility>
#include <vector>

int wmain(int argc, wchar_t* argv[]) {
    (void)SetConsoleCP(CP_UTF8);
    (void)SetConsoleOutputCP(CP_UTF8);

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1,
                                                  nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return 2;
        }
        std::string converted(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], -1,
                                converted.data(), required, nullptr, nullptr) != required) {
            return 2;
        }
        converted.resize(static_cast<std::size_t>(required - 1));
        arguments.push_back(std::move(converted));
    }

    std::vector<char*> raw_arguments;
    raw_arguments.reserve(arguments.size());
    for (std::string& argument : arguments) {
        raw_arguments.push_back(argument.data());
    }
    return cppapplab::run_cli(argc, raw_arguments.data());
}
#else
int main(int argc, char* argv[]) {
    return cppapplab::run_cli(argc, argv);
}
#endif
