#include "process_runner.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

namespace cppapplab::system_lab {

bool write_all(int file_descriptor, const char* data, std::size_t size) {
    // TODO(W5-PROCESS-1): handle partial writes and retry EINTR.
    (void)file_descriptor;
    (void)data;
    (void)size;
    return false;
}

ProcessResult run_process(
    const std::vector<std::string>& arguments,
    std::chrono::milliseconds timeout,
    std::size_t output_limit
) {
    // TODO(W5-PROCESS-2): validate arguments, create a pipe, fork, and execvp.
    // TODO(W5-PROCESS-3): close unused descriptors in both parent and child.
    // TODO(W5-PROCESS-4): capture output without invoking a shell.
    // TODO(W5-PROCESS-5): use waitpid and enforce the child timeout.
    // TODO(W5-PROCESS-6): cap output while continuing to drain the pipe.
    (void)arguments;
    (void)timeout;
    (void)output_limit;
    return {};
}

}  // namespace cppapplab::system_lab
