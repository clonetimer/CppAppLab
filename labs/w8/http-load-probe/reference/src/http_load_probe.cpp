#include "http_load_probe.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace cppapplab::http {
namespace {

using Clock = std::chrono::steady_clock;

class UniqueFd {
   public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    ~UniqueFd() {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }
    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

   private:
    int fd_;
};

void validate_config(const HttpLoadProbeConfig& config) {
    in_addr address{};
    if (::inet_pton(AF_INET, config.host.c_str(), &address) != 1 || config.port == 0U) {
        throw std::invalid_argument("load probe requires an IPv4 host and non-zero port");
    }
    if (config.target.empty() || config.target.front() != '/' ||
        config.target.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("load probe target must be an absolute HTTP path");
    }
    if (config.client_count == 0U || config.client_count > 128U ||
        config.requests_per_client == 0U || config.requests_per_client > 10000U) {
        throw std::invalid_argument("load probe client and request counts are outside their limits");
    }
    if (config.timeout <= std::chrono::milliseconds::zero() ||
        config.timeout > std::chrono::seconds(30)) {
        throw std::invalid_argument("load probe timeout must be in the range 1 ms..30 s");
    }
    if (config.expected_status_code < 100 || config.expected_status_code > 599) {
        throw std::invalid_argument("expected HTTP status must be in the range 100..599");
    }
}

[[nodiscard]] bool wait_for_fd(int fd, short events, std::chrono::milliseconds timeout) {
    pollfd descriptor{};
    descriptor.fd = fd;
    descriptor.events = events;
    const auto deadline = Clock::now() + timeout;
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - Clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            return false;
        }
        const auto bounded = std::min<std::int64_t>(remaining.count(), 30000);
        const int result = ::poll(&descriptor, 1, static_cast<int>(bounded));
        if (result > 0) {
            return (descriptor.revents & (events | POLLERR | POLLHUP)) != 0;
        }
        if (result == 0) {
            return false;
        }
        if (errno != EINTR) {
            return false;
        }
    }
}

[[nodiscard]] UniqueFd connect_client(const HttpLoadProbeConfig& config) {
    UniqueFd socket_fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
    if (socket_fd.get() < 0) {
        return UniqueFd{};
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config.port);
    if (::inet_pton(AF_INET, config.host.c_str(), &address.sin_addr) != 1) {
        return UniqueFd{};
    }

    const int result =
        ::connect(socket_fd.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    if (result < 0 && errno != EINPROGRESS) {
        return UniqueFd{};
    }
    if (result < 0 && !wait_for_fd(socket_fd.get(), POLLOUT, config.timeout)) {
        return UniqueFd{};
    }

    int socket_error = 0;
    socklen_t error_size = sizeof(socket_error);
    if (::getsockopt(socket_fd.get(), SOL_SOCKET, SO_ERROR, &socket_error, &error_size) < 0 ||
        socket_error != 0) {
        return UniqueFd{};
    }

    return socket_fd;
}

[[nodiscard]] bool send_all(int fd, std::string_view bytes,
                            std::chrono::milliseconds timeout) {
    std::size_t offset = 0U;
    const auto deadline = Clock::now() + timeout;
    while (offset < bytes.size()) {
        const ssize_t sent =
            ::send(fd, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent > 0) {
            offset += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - Clock::now());
            if (remaining <= std::chrono::milliseconds::zero() ||
                !wait_for_fd(fd, POLLOUT, remaining)) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

struct ParsedResponse {
    int status_code{0};
    std::size_t wire_bytes{0U};
};

[[nodiscard]] bool read_response(int fd, std::chrono::milliseconds timeout,
                                 ParsedResponse& response) {
    std::string buffer;
    const auto deadline = Clock::now() + timeout;
    std::size_t header_end = std::string::npos;
    while ((header_end = buffer.find("\r\n\r\n")) == std::string::npos) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - Clock::now());
        if (remaining <= std::chrono::milliseconds::zero() ||
            !wait_for_fd(fd, POLLIN, remaining)) {
            return false;
        }
        char chunk[4096];
        const ssize_t received = ::recv(fd, chunk, sizeof(chunk), MSG_DONTWAIT);
        if (received > 0) {
            buffer.append(chunk, static_cast<std::size_t>(received));
        } else if (received < 0 && errno == EINTR) {
            continue;
        } else if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        } else {
            return false;
        }
    }

    const std::size_t line_end = buffer.find("\r\n");
    if (line_end == std::string::npos) {
        return false;
    }
    const std::string_view status_line(buffer.data(), line_end);
    const std::size_t first_space = status_line.find(' ');
    if (first_space == std::string_view::npos || first_space + 4U > status_line.size()) {
        return false;
    }
    try {
        response.status_code = std::stoi(std::string(status_line.substr(first_space + 1U, 3U)));
    } catch (...) {
        return false;
    }

    std::size_t content_length = 0U;
    bool found_content_length = false;
    std::size_t cursor = line_end + 2U;
    while (cursor < header_end) {
        const std::size_t next = buffer.find("\r\n", cursor);
        if (next == std::string::npos || next > header_end) {
            return false;
        }
        const std::string_view line(buffer.data() + cursor, next - cursor);
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string name(line.substr(0U, colon));
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
                if (value >= 'A' && value <= 'Z') {
                    return static_cast<char>(value - 'A' + 'a');
                }
                return static_cast<char>(value);
            });
            if (name == "content-length") {
                try {
                    content_length = static_cast<std::size_t>(
                        std::stoull(std::string(line.substr(colon + 1U))));
                } catch (...) {
                    return false;
                }
                found_content_length = true;
            }
        }
        cursor = next + 2U;
    }
    if (!found_content_length || content_length > 16U * 1024U * 1024U) {
        return false;
    }

    const std::size_t expected_size = header_end + 4U + content_length;
    while (buffer.size() < expected_size) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - Clock::now());
        if (remaining <= std::chrono::milliseconds::zero() ||
            !wait_for_fd(fd, POLLIN, remaining)) {
            return false;
        }
        char chunk[8192];
        const ssize_t received = ::recv(fd, chunk, sizeof(chunk), MSG_DONTWAIT);
        if (received > 0) {
            buffer.append(chunk, static_cast<std::size_t>(received));
        } else if (received < 0 && errno == EINTR) {
            continue;
        } else if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        } else {
            return false;
        }
    }

    response.wire_bytes = expected_size;
    return true;
}

[[nodiscard]] std::string make_request(const HttpLoadProbeConfig& config, bool close) {
    std::string request = "GET ";
    request += config.target;
    request += " HTTP/1.1\r\nHost: ";
    request += config.host;
    request += "\r\nConnection: ";
    request += close ? "close\r\n\r\n" : "keep-alive\r\n\r\n";
    return request;
}

[[nodiscard]] std::uint64_t percentile(const std::vector<std::uint64_t>& sorted,
                                       std::uint64_t numerator) {
    if (sorted.empty()) {
        return 0U;
    }
    const std::uint64_t count = static_cast<std::uint64_t>(sorted.size());
    const std::uint64_t rank = (numerator * count + 99U) / 100U;
    const std::uint64_t index = std::max<std::uint64_t>(1U, rank) - 1U;
    return sorted[static_cast<std::size_t>(std::min(index, count - 1U))];
}

}  // namespace

HttpLoadProbeResult run_http_load_probe(const HttpLoadProbeConfig& config) {
    validate_config(config);

    const auto started = Clock::now();
    const std::uint64_t total_requests =
        static_cast<std::uint64_t>(config.client_count) *
        static_cast<std::uint64_t>(config.requests_per_client);
    std::atomic<std::uint64_t> succeeded{0U};
    std::atomic<std::uint64_t> failed{0U};
    std::mutex latency_mutex;
    std::vector<std::uint64_t> latencies;
    latencies.reserve(static_cast<std::size_t>(total_requests));

    std::vector<std::thread> clients;
    clients.reserve(config.client_count);
    for (std::size_t client_index = 0U; client_index < config.client_count; ++client_index) {
        clients.emplace_back([&] {
            UniqueFd socket_fd = connect_client(config);
            if (socket_fd.get() < 0) {
                failed.fetch_add(static_cast<std::uint64_t>(config.requests_per_client));
                return;
            }

            for (std::size_t request_index = 0U;
                 request_index < config.requests_per_client; ++request_index) {
                const bool last_request = request_index + 1U == config.requests_per_client;
                const std::string wire = make_request(config, last_request);
                const auto request_started = Clock::now();
                if (!send_all(socket_fd.get(), wire, config.timeout)) {
                    failed.fetch_add(
                        static_cast<std::uint64_t>(config.requests_per_client - request_index));
                    return;
                }

                ParsedResponse response;
                if (!read_response(socket_fd.get(), config.timeout, response)) {
                    failed.fetch_add(
                        static_cast<std::uint64_t>(config.requests_per_client - request_index));
                    return;
                }

                const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    Clock::now() - request_started);
                const auto latency_us =
                    static_cast<std::uint64_t>(std::max<std::int64_t>(1, latency.count()));
                {
                    const std::lock_guard<std::mutex> lock(latency_mutex);
                    latencies.push_back(latency_us);
                }
                if (response.status_code == config.expected_status_code) {
                    succeeded.fetch_add(1U);
                } else {
                    failed.fetch_add(1U);
                }
            }
        });
    }
    for (auto& client : clients) {
        client.join();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - started);
    const auto elapsed_us =
        static_cast<std::uint64_t>(std::max<std::int64_t>(1, elapsed.count()));

    std::sort(latencies.begin(), latencies.end());
    HttpLoadProbeResult result;
    result.requests_attempted = total_requests;
    result.requests_succeeded = succeeded.load();
    result.requests_failed = failed.load();
    result.elapsed_microseconds = elapsed_us;
    result.requests_per_second =
        static_cast<double>(result.requests_succeeded) * 1000000.0 /
        static_cast<double>(elapsed_us);
    result.p50_latency_microseconds = percentile(latencies, 50U);
    result.p95_latency_microseconds = percentile(latencies, 95U);
    result.p99_latency_microseconds = percentile(latencies, 99U);
    return result;
}

}  // namespace cppapplab::http
