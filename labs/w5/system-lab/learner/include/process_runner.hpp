#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace cppapplab::system_lab {

struct ProcessResult {
    int exit_code{-1};
    bool exited{false};
    bool signaled{false};
    int signal_number{0};
    std::string output;
};

[[nodiscard]] ProcessResult run_process(const std::vector<std::string>& arguments,
                                        std::chrono::milliseconds timeout = std::chrono::seconds(5),
                                        std::size_t output_limit = 1024U * 1024U);

[[nodiscard]] bool write_all(int file_descriptor, const char* data, std::size_t size);

}  // namespace cppapplab::system_lab
