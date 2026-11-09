#include "process_runner.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cppapplab::system_lab {

bool write_all(int file_descriptor, const char* data, std::size_t size) {
    std::size_t written = 0U;
    while (written < size) {
        const ssize_t result = ::write(file_descriptor, data + written, size - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

ProcessResult run_process(const std::vector<std::string>& arguments,
                          std::chrono::milliseconds timeout, std::size_t output_limit) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::invalid_argument("process arguments must contain a program name");
    }
    if (timeout.count() <= 0) {
        throw std::invalid_argument("process timeout must be positive");
    }
    if (output_limit == 0U) {
        throw std::invalid_argument("process output limit must be positive");
    }

    int pipe_descriptors[2]{-1, -1};
    if (::pipe2(pipe_descriptors, O_CLOEXEC) != 0) {
        throw std::runtime_error("pipe2 failed: " + std::string(std::strerror(errno)));
    }

    const int read_flags = ::fcntl(pipe_descriptors[0], F_GETFL, 0);
    if (read_flags < 0 || ::fcntl(pipe_descriptors[0], F_SETFL, read_flags | O_NONBLOCK) < 0) {
        const int saved_errno = errno;
        ::close(pipe_descriptors[0]);
        ::close(pipe_descriptors[1]);
        throw std::runtime_error("fcntl failed: " + std::string(std::strerror(saved_errno)));
    }

    const pid_t child = ::fork();
    if (child < 0) {
        const int saved_errno = errno;
        ::close(pipe_descriptors[0]);
        ::close(pipe_descriptors[1]);
        throw std::runtime_error("fork failed: " + std::string(std::strerror(saved_errno)));
    }

    if (child == 0) {
        ::close(pipe_descriptors[0]);
        if (::dup2(pipe_descriptors[1], STDOUT_FILENO) < 0 ||
            ::dup2(pipe_descriptors[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        ::close(pipe_descriptors[1]);

        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1U);
        for (const auto& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);
        ::execvp(argv.front(), argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    ::close(pipe_descriptors[1]);
    ProcessResult result;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool pipe_open = true;
    bool child_reaped = false;
    int wait_status = 0;
    char buffer[4096];

    while (pipe_open || !child_reaped) {
        if (!child_reaped) {
            const pid_t waited = ::waitpid(child, &wait_status, WNOHANG);
            if (waited == child) {
                child_reaped = true;
            } else if (waited < 0 && errno != EINTR) {
                ::close(pipe_descriptors[0]);
                throw std::runtime_error("waitpid failed: " + std::string(std::strerror(errno)));
            }
        }

        if (std::chrono::steady_clock::now() >= deadline && !child_reaped) {
            ::kill(child, SIGKILL);
            while (::waitpid(child, &wait_status, 0) < 0 && errno == EINTR) {
            }
            child_reaped = true;
            result.signaled = true;
            result.signal_number = SIGKILL;
            result.exit_code = 128 + SIGKILL;
        }

        if (pipe_open) {
            pollfd descriptor{pipe_descriptors[0], POLLIN | POLLHUP, 0};
            const int polled = ::poll(&descriptor, 1, 10);
            if (polled < 0 && errno != EINTR) {
                ::close(pipe_descriptors[0]);
                throw std::runtime_error("poll failed: " + std::string(std::strerror(errno)));
            }
            if (polled > 0) {
                for (;;) {
                    const ssize_t count = ::read(pipe_descriptors[0], buffer, sizeof(buffer));
                    if (count > 0) {
                        const std::size_t available = output_limit > result.output.size()
                                                          ? output_limit - result.output.size()
                                                          : 0U;
                        result.output.append(buffer,
                                             std::min(available, static_cast<std::size_t>(count)));
                    } else if (count == 0) {
                        pipe_open = false;
                        break;
                    } else if (errno == EINTR) {
                        continue;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    } else {
                        pipe_open = false;
                        break;
                    }
                }
            }
        }
    }
    ::close(pipe_descriptors[0]);

    if (!result.signaled) {
        if (WIFEXITED(wait_status)) {
            result.exited = true;
            result.exit_code = WEXITSTATUS(wait_status);
        } else if (WIFSIGNALED(wait_status)) {
            result.signaled = true;
            result.signal_number = WTERMSIG(wait_status);
            result.exit_code = 128 + result.signal_number;
        }
    }
    return result;
}

}  // namespace cppapplab::system_lab
