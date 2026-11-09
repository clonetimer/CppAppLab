#include "cppapplab/process.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace cppapplab {
namespace {

[[noreturn]] void throw_system_error(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

void append_limited(ProcessResult& result, const char* data, std::size_t size, std::size_t limit) {
    const std::size_t available = result.output.size() < limit ? limit - result.output.size() : 0;
    const std::size_t keep = size < available ? size : available;

    if (keep > 0) {
        result.output.append(data, keep);
    }
    if (keep < size) {
        result.output_truncated = true;
    }
}

}  // namespace

ProcessResult run_process(const std::vector<std::string>& arguments,
                          const std::filesystem::path& working_directory,
                          std::chrono::milliseconds timeout, std::size_t max_output_bytes) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::invalid_argument("run_process requires a non-empty executable");
    }
    if (timeout.count() <= 0) {
        throw std::invalid_argument("run_process timeout must be positive");
    }

    std::vector<char*> raw_arguments;
    raw_arguments.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments) {
        raw_arguments.push_back(const_cast<char*>(argument.c_str()));
    }
    raw_arguments.push_back(nullptr);

    int pipe_fds[2]{};
    if (::pipe(pipe_fds) != 0) {
        throw_system_error("pipe");
    }

    const pid_t child = ::fork();
    if (child < 0) {
        const int saved_errno = errno;
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        errno = saved_errno;
        throw_system_error("fork");
    }

    if (child == 0) {
        (void)::setpgid(0, 0);
        ::close(pipe_fds[0]);

        const int null_input = ::open("/dev/null", O_RDONLY);
        if (null_input < 0 || ::dup2(null_input, STDIN_FILENO) < 0 ||
            ::dup2(pipe_fds[1], STDOUT_FILENO) < 0 || ::dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        ::close(null_input);
        ::close(pipe_fds[1]);

        if (!working_directory.empty() && ::chdir(working_directory.c_str()) != 0) {
            std::perror("chdir");
            _exit(126);
        }

        ::execvp(raw_arguments[0], raw_arguments.data());
        std::perror("execvp");
        _exit(127);
    }

    (void)::setpgid(child, child);
    ::close(pipe_fds[1]);

    const int flags = ::fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags < 0 || ::fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        const int saved_errno = errno;
        (void)::kill(-child, SIGKILL);
        (void)::kill(child, SIGKILL);
        int ignored_status = 0;
        (void)::waitpid(child, &ignored_status, 0);
        ::close(pipe_fds[0]);
        errno = saved_errno;
        throw_system_error("fcntl");
    }

    ProcessResult result;
    int child_status = 0;
    bool child_finished = false;
    bool pipe_closed = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!child_finished || !pipe_closed) {
        if (std::chrono::steady_clock::now() >= deadline && (!child_finished || !pipe_closed)) {
            result.timed_out = true;
            (void)::kill(-child, SIGKILL);
            (void)::kill(child, SIGKILL);
        }

        pollfd descriptor{};
        descriptor.fd = pipe_fds[0];
        descriptor.events = POLLIN | POLLHUP;

        const int poll_result = ::poll(&descriptor, 1, 50);
        if (poll_result < 0 && errno != EINTR) {
            const int saved_errno = errno;
            (void)::kill(-child, SIGKILL);
            (void)::kill(child, SIGKILL);
            if (!child_finished) {
                (void)::waitpid(child, &child_status, 0);
            }
            ::close(pipe_fds[0]);
            errno = saved_errno;
            throw_system_error("poll");
        }

        if (poll_result > 0 && (descriptor.revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            while (true) {
                char buffer[4096];
                const ssize_t count = ::read(pipe_fds[0], buffer, sizeof(buffer));

                if (count > 0) {
                    append_limited(result, buffer, static_cast<std::size_t>(count),
                                   max_output_bytes);
                    continue;
                }
                if (count == 0) {
                    pipe_closed = true;
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                const int saved_errno = errno;
                (void)::kill(-child, SIGKILL);
                (void)::kill(child, SIGKILL);
                if (!child_finished) {
                    (void)::waitpid(child, &child_status, 0);
                }
                ::close(pipe_fds[0]);
                errno = saved_errno;
                throw_system_error("read");
            }
        }

        if (!child_finished) {
            const pid_t waited = ::waitpid(child, &child_status, WNOHANG);
            if (waited == child) {
                child_finished = true;
            } else if (waited < 0 && errno != EINTR) {
                const int saved_errno = errno;
                (void)::kill(-child, SIGKILL);
                (void)::kill(child, SIGKILL);
                ::close(pipe_fds[0]);
                errno = saved_errno;
                throw_system_error("waitpid");
            }
        }
    }

    ::close(pipe_fds[0]);

    if (WIFEXITED(child_status)) {
        result.exit_code = WEXITSTATUS(child_status);
    } else if (WIFSIGNALED(child_status)) {
        result.exit_code = 128 + WTERMSIG(child_status);
    }

    return result;
}

}  // namespace cppapplab
