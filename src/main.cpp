#include "cppapplab/learning.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "cppapplab/process.hpp"
#include "windows_state_io.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int run_native_modern_step(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    namespace state = cppapplab::windows_state_io;
    using namespace std::chrono_literals;
    try {
        if (args.size() != 6U || (args[4] != "20" && args[4] != "23") ||
            (args[5] != "0" && args[5] != "1")) {
            throw std::invalid_argument("Invalid fixed native experiment step.");
        }
        const auto cache = state::environment_path(L"CPPAPPLAB_NATIVE_BUILD_ROOT");
        const auto sysroot = state::environment_path(L"CPPAPPLAB_NATIVE_SYSROOT");
        const auto root = fs::u8path(args[2]);
        if (!cache || !sysroot || !root.is_absolute() || !cache->is_absolute() ||
            !sysroot->is_absolute() ||
            root.filename().wstring().rfind(L"cppapplab-modern-", 0) != 0) {
            throw std::invalid_argument("The verified native package and a private probe directory are required.");
        }
        state::require_directory(root);
        state::require_directory(*cache / "tmp");
        state::require_directory(*sysroot);
        if (!fs::equivalent(root.parent_path(), *cache / "tmp")) {
            throw std::invalid_argument("Experiment must run in the package's private temporary cache.");
        }
        const auto compiler = *sysroot / "bin/g++.exe";
        if (!state::regular_exists(compiler) || !state::regular_exists(*sysroot / "include/wchar.h") ||
            !state::regular_exists(*sysroot / "include/windows.h")) {
            throw std::runtime_error("Incomplete bundled native compiler.");
        }
        const auto& phase = args[3];
        std::vector<std::string> command{compiler.u8string()};
        auto timeout = 15s;
        if (phase == "toolchain") {
            command.insert(command.end(), {"-dumpfullversion", "-dumpversion"});
            timeout = 5s;
        } else if (phase == "target") {
            command.push_back("-dumpmachine");
            timeout = 5s;
        } else if (phase == "feature-check" || phase == "compile") {
            const std::string source = phase == "feature-check" ? "feature.cpp" : "main.cpp";
            if (!state::regular_exists(root / source)) {
                throw std::runtime_error("Missing fixed experiment source.");
            }
            command.push_back("--sysroot=" + sysroot->generic_u8string());
            command.push_back(args[4] == "20" ? "-std=c++20" : "-std=c++23");
            if (phase == "feature-check") {
                command.insert(command.end(), {"-fsyntax-only", source});
            } else {
                command.insert(command.end(), {"-Wall", "-Wextra", "-pedantic-errors",
                                               "-fdiagnostics-color=never"});
                if (args[5] == "1") {
                    command.push_back("-pthread");
                }
                command.insert(command.end(), {source, "-o", "probe.exe"});
            }
        } else if (phase == "project-configure" || phase == "project-build" ||
                   phase == "project-test") {
            if (root.filename().wstring().rfind(L"cppapplab-modern-check-", 0) != 0 ||
                args[5] != "0") {
                throw std::invalid_argument("Fixed modern project staging directory required.");
            }
            for (const auto* filename : {"CMakeLists.txt", "contract.hpp", "tests.cpp",
                                         "implementation.cpp"}) {
                if (!state::regular_exists(root / filename)) {
                    throw std::runtime_error("Missing fixed modern project input.");
                }
            }
            const auto cmake = *sysroot / "bin/cmake.exe";
            const auto ninja = *sysroot / "bin/ninja.exe";
            const auto ctest = *sysroot / "bin/ctest.exe";
            if (!state::regular_exists(cmake) || !state::regular_exists(ninja) ||
                !state::regular_exists(ctest)) {
                throw std::runtime_error("Incomplete bundled CMake tools.");
            }
            if (phase == "project-configure") {
                if (state::entry_exists(root / "build")) {
                    throw std::runtime_error("Modern project requires a fresh build directory.");
                }
                command = {cmake.u8string(), "-S", ".", "-B", "build", "-G", "Ninja",
                           "-DCMAKE_CXX_COMPILER:FILEPATH=" + compiler.generic_u8string(),
                           "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + ninja.generic_u8string(),
                           "-DCMAKE_SYSROOT:PATH=" + sysroot->generic_u8string(),
                           "-DCMAKE_PREFIX_PATH:PATH=" + sysroot->generic_u8string(),
                           "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE",
                           "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE",
                           "-DCPPAPPLAB_NATIVE_MODERN_PROFILE=ucrt64-gcc16"};
                timeout = 30s;
            } else {
                state::require_directory(root / "build");
                if (!state::regular_exists(root / "build/CMakeCache.txt")) {
                    throw std::runtime_error("Missing configured modern project cache.");
                }
                if (phase == "project-build") {
                    command = {cmake.u8string(), "--build", "build", "--parallel", "2"};
                    timeout = 90s;
                } else {
                    if (!state::regular_exists(root / "build/project_tests.exe")) {
                        throw std::runtime_error("Missing built modern project test executable.");
                    }
                    command = {ctest.u8string(), "--test-dir", "build", "--no-tests=error",
                               "--output-on-failure"};
                    timeout = 45s;
                }
            }
        } else if (phase == "run") {
            if (!state::regular_exists(root / "probe.exe")) {
                throw std::runtime_error("Missing compiled experiment executable.");
            }
            command = {(root / "probe.exe").u8string()};
            timeout = 3s;
        } else {
            throw std::invalid_argument("Unknown fixed experiment phase.");
        }
        const auto result = cppapplab::run_process(command, root, timeout, 64U * 1024U);
        std::cout << "CPPAPPLAB_NATIVE_PROBE_V1 " << result.exit_code << ' '
                  << (result.timed_out ? 1 : 0) << ' ' << (result.output_truncated ? 1 : 0)
                  << '\n' << result.output;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native experiment unavailable: " << error.what() << '\n';
        return 2;
    }
}

}  // namespace

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

    if (arguments.size() > 1U && arguments[1] == "--native-modern-step") {
        return run_native_modern_step(arguments);
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
