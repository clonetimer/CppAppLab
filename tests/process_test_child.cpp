#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

int parse_milliseconds(const char* value) {
    if (value == nullptr || *value == '\0') {
        return -1;
    }

    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0 || parsed > 60000) {
        return -1;
    }
    return static_cast<int>(parsed);
}

int sleep_for(const char* value) {
    const int milliseconds = parse_milliseconds(value);
    if (milliseconds < 0) {
        return 2;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
}

int copy_standard_input() {
    char buffer[4096];
    while (std::cin) {
        std::cin.read(buffer, sizeof(buffer));
        const std::streamsize count = std::cin.gcount();
        if (count > 0) {
            std::cout.write(buffer, count);
            if (!std::cout) {
                return 3;
            }
        }
    }
    return std::cin.eof() ? 0 : 4;
}

int spawn_wait(const char* duration, const char* executable_argument) {
    const int milliseconds = parse_milliseconds(duration);
    if (milliseconds < 0 || executable_argument == nullptr) {
        return 2;
    }

#ifdef _WIN32
    (void)executable_argument;
    std::wstring executable(32768, L'\0');
    const DWORD executable_length =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (executable_length == 0 || executable_length >= executable.size()) {
        return 125;
    }
    executable.resize(executable_length);

    // argv[0] must identify the executable; quote it because the build path may contain spaces.
    std::wstring command_line = L"\"" + executable + L"\" child-sleep " +
                                std::to_wstring(milliseconds);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(executable.c_str(), command_line.data(), nullptr, nullptr, TRUE,
                        CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup, &process)) {
        return 125;
    }

    (void)CloseHandle(process.hThread);
    const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 125;
    if (wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exit_code)) {
        (void)CloseHandle(process.hProcess);
        return static_cast<int>(exit_code);
    }
    (void)CloseHandle(process.hProcess);
    return 125;
#else
    const std::filesystem::path executable = std::filesystem::absolute(executable_argument);
    const pid_t child = ::fork();
    if (child < 0) {
        return 125;
    }
    if (child == 0) {
        const std::string child_path = executable.string();
        const std::string child_duration = std::to_string(milliseconds);
        ::execl(child_path.c_str(), child_path.c_str(), "child-sleep", child_duration.c_str(),
                static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return 125;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 128;
#endif
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
    (void)_setmode(_fileno(stderr), _O_BINARY);
#endif

    if (argc < 2) {
        std::cerr << "usage: process-test-child <emit|copy-stdin|sleep|child-sleep|spawn-wait|exit>\n";
        return 2;
    }

    const std::string command = argv[1];
    if (command == "emit" && argc == 3) {
        std::cout << argv[2] << '\n';
        return std::cout ? 0 : 3;
    }
    if (command == "copy-stdin" && argc == 2) {
        return copy_standard_input();
    }
    if ((command == "sleep" || command == "child-sleep") && argc == 3) {
        return sleep_for(argv[2]);
    }
    if (command == "spawn-wait" && argc == 3) {
        return spawn_wait(argv[2], argv[0]);
    }
    if (command == "exit" && argc == 3) {
        const int code = parse_milliseconds(argv[2]);
        return code < 0 ? 2 : code;
    }

    std::cerr << "invalid process-test-child arguments\n";
    return 2;
}
