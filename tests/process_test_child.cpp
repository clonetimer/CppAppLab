#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void use_binary_standard_streams() {
#ifdef _WIN32
    (void)::_setmode(::_fileno(stdin), _O_BINARY);
    (void)::_setmode(::_fileno(stdout), _O_BINARY);
    (void)::_setmode(::_fileno(stderr), _O_BINARY);
#endif
}

unsigned long parse_unsigned(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return 0UL;
    }
    const unsigned long parsed = std::stoul(value);
    return parsed;
}

int spawn_and_wait(const char* executable, unsigned long milliseconds) {
#ifdef _WIN32
    const std::filesystem::path executable_path = std::filesystem::absolute(executable);
    std::wstring command_line = L"\"" + executable_path.wstring() + L"\" sleep " +
                                std::to_wstring(milliseconds);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!::CreateProcessW(executable_path.c_str(), command_line.data(), nullptr, nullptr, FALSE,
                          CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return 91;
    }

    const DWORD wait_result = ::WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    const BOOL got_exit_code = ::GetExitCodeProcess(process.hProcess, &exit_code);
    (void)::CloseHandle(process.hThread);
    (void)::CloseHandle(process.hProcess);
    return wait_result == WAIT_OBJECT_0 && got_exit_code && exit_code == 0U ? 0 : 92;
#else
    const pid_t child = ::fork();
    if (child < 0) {
        return 91;
    }
    if (child == 0) {
        const std::string delay = std::to_string(milliseconds);
        ::execl(executable, executable, "sleep", delay.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        // EINTR is the only expected transient wait failure in this test helper.
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 92;
#endif
}

int copy_standard_input() {
    char buffer[4096];
    while (std::cin) {
        std::cin.read(buffer, static_cast<std::streamsize>(sizeof(buffer)));
        const std::streamsize count = std::cin.gcount();
        if (count > 0) {
            std::cout.write(buffer, count);
            if (!std::cout) {
                return 93;
            }
        }
    }
    return std::cin.eof() ? 0 : 94;
}

}  // namespace

int main(int argc, char* argv[]) {
    use_binary_standard_streams();

    if (argc < 2) {
        std::cerr << "usage: process_test_child <mode> [arguments...]\n";
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "emit" && argc == 3) {
        std::cout << argv[2] << '\n';
        return 0;
    }
    if (mode == "emit-stderr" && argc == 3) {
        std::cerr << argv[2] << '\n';
        return 0;
    }
    if (mode == "copy-stdin" && argc == 2) {
        return copy_standard_input();
    }
    if (mode == "sleep" && argc == 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(parse_unsigned(argv[2])));
        return 0;
    }
    if (mode == "spawn-child" && argc == 3) {
        return spawn_and_wait(argv[0], parse_unsigned(argv[2]));
    }
    if (mode == "exit" && argc == 3) {
        const unsigned long code = parse_unsigned(argv[2]);
        return code <= 255UL ? static_cast<int>(code) : 95;
    }
    if (mode == "write" && argc == 3) {
        std::cout << argv[2];
        return std::cout ? 0 : 96;
    }
    if (mode == "write-repeat" && argc == 3) {
        const unsigned long total = parse_unsigned(argv[2]);
        const std::string block(4096U, 'x');
        unsigned long remaining = total;
        while (remaining > 0UL) {
            const std::size_t count = static_cast<std::size_t>(
                std::min<unsigned long>(remaining, static_cast<unsigned long>(block.size())));
            std::cout.write(block.data(), static_cast<std::streamsize>(count));
            if (!std::cout) {
                return 96;
            }
            remaining -= static_cast<unsigned long>(count);
        }
        return 0;
    }
    if (mode == "join-args" && argc >= 2) {
        for (int index = 2; index < argc; ++index) {
            std::cout << '[' << argv[index] << "]\n";
        }
        return 0;
    }
    if (mode == "write-cwd-marker" && argc == 2) {
        std::ofstream marker("process-runner-child.marker", std::ios::binary);
        marker << "working directory verified\n";
        return marker ? 0 : 97;
    }

    std::cerr << "unknown process_test_child mode: " << mode << '\n';
    return 2;
}
