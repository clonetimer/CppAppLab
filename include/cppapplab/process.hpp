#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace cppapplab {

struct ProcessResult {
    int exit_code{-1};
    bool timed_out{false};
    bool output_truncated{false};
    std::string output;
};

[[nodiscard]] ProcessResult run_process(const std::vector<std::string>& arguments,
                                        const std::filesystem::path& working_directory,
                                        std::chrono::milliseconds timeout,
                                        std::size_t max_output_bytes);

}  // namespace cppapplab
