#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

std::filesystem::path current_executable_path(const char* argv0) {
#ifdef _WIN32
    (void)argv0;
    std::wstring buffer(32768U, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(),
                                               static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) {
        throw std::runtime_error("Cannot determine the process test executable path.");
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
#else
    return std::filesystem::absolute(std::filesystem::path(argv0));
#endif
}

void configure_process_test_child(const char* argv0) {
    std::filesystem::path child =
        current_executable_path(argv0).parent_path() / "cppapplab_process_test_child";
#ifdef _WIN32
    child += ".exe";
    if (::_wputenv_s(L"CPPAPPLAB_PROCESS_TEST_CHILD", child.c_str()) != 0) {
        throw std::runtime_error("Cannot set CPPAPPLAB_PROCESS_TEST_CHILD.");
    }
#else
    const std::string child_path = child.string();
    if (::setenv("CPPAPPLAB_PROCESS_TEST_CHILD", child_path.c_str(), 1) != 0) {
        throw std::runtime_error("Cannot set CPPAPPLAB_PROCESS_TEST_CHILD.");
    }
#endif
}

}  // namespace

int main(int argc, char** argv) {
    try {
        configure_process_test_child(argv[0]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Process test setup failed: %s\n", error.what());
        return 2;
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
