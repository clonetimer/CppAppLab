#include "http_server.hpp"

#include "http_request_parser.hpp"
#include "static_file_handler.hpp"
#include "thread_pool.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cppapplab::http {
namespace {

constexpr std::uint64_t kListenerToken = 1U;
constexpr std::uint64_t kWakeToken = 2U;
constexpr std::uint64_t kTimerToken = 3U;
constexpr std::uint64_t kFirstConnectionToken = 4U;
constexpr std::size_t kResponseHeaderReserve = 256U;

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

    [[nodiscard]] int release() noexcept {
        const int value = fd_;
        fd_ = -1;
        return value;
    }

   private:
    int fd_;
};

[[noreturn]] void throw_system_error(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}

[[nodiscard]] HttpServerConfig validate_config(HttpServerConfig config) {
    constexpr auto kMaximumTimeout = std::chrono::hours(24);
    const auto valid_timeout = [kMaximumTimeout](std::chrono::milliseconds timeout) {
        return timeout.count() > 0 && timeout <= kMaximumTimeout;
    };
    if (config.backlog <= 0 || config.worker_count == 0U || config.task_queue_capacity == 0U ||
        config.max_connections == 0U || config.max_request_bytes == 0U ||
        config.max_response_bytes < 512U || config.max_requests_per_connection == 0U ||
        config.max_persistent_connections == 0U || !valid_timeout(config.idle_timeout) ||
        !valid_timeout(config.header_timeout) || !valid_timeout(config.body_timeout)) {
        throw std::invalid_argument("HTTP server limits and timeouts must be positive and bounded");
    }

    in_addr parsed{};
    if (::inet_pton(AF_INET, config.bind_address.c_str(), &parsed) != 1) {
        throw std::invalid_argument("bind_address must be an IPv4 literal");
    }
    return config;
}

[[nodiscard]] HttpResponse error_response(int status, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status;
    response.reason_phrase = std::move(reason);
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.body = std::move(body);
    return response;
}

[[nodiscard]] bool is_too_large_error(const std::string& message) {
    return message.find("too large") != std::string::npos ||
           message.find("limit") != std::string::npos ||
           message.find("message size") != std::string::npos;
}

void send_best_effort(int fd, const std::string& bytes) noexcept {
    std::size_t offset = 0U;
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
        return;
    }
}

}  // namespace

struct HttpServer::Impl {
    using Clock = std::chrono::steady_clock;

    enum class DeadlineKind { None, Idle, Header, Body };

    struct DeadlineEntry {
        Clock::time_point when;
        std::uint64_t token{0U};
        std::uint64_t generation{0U};
        DeadlineKind kind{DeadlineKind::None};
    };

    struct DeadlineLater {
        [[nodiscard]] bool operator()(const DeadlineEntry& left,
                                      const DeadlineEntry& right) const noexcept {
            return left.when > right.when;
        }
    };

    struct Connection {
        int fd{-1};
        std::uint64_t token{0U};
        std::string read_buffer;
        std::string write_buffer;
        std::size_t write_offset{0U};
        bool processing{false};
        bool peer_closed{false};
        bool close_after_write{false};
        ConnectionPolicy pending_policy{ConnectionPolicy::Close};
        std::string response_version{"HTTP/1.1"};
        std::size_t requests_started{0U};
        bool persistent_slot_reserved{false};
        DeadlineKind deadline_kind{DeadlineKind::None};
        std::uint64_t deadline_generation{0U};
        Clock::time_point deadline_at{};
    };

    struct Completion {
        int fd{-1};
        std::uint64_t token{0U};
        HttpResponse response;
    };

    explicit Impl(HttpServerConfig server_config, Router server_router)
        : config(validate_config(std::move(server_config))),
          router(std::move(server_router)),
          static_files(config.document_root, config.max_response_bytes - kResponseHeaderReserve),
          pool(config.worker_count, config.task_queue_capacity) {
        UniqueFd listener(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        if (listener.get() < 0) {
            throw_system_error("socket");
        }

        const int reuse = 1;
        if (::setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            throw_system_error("setsockopt(SO_REUSEADDR)");
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(config.port);
        if (::inet_pton(AF_INET, config.bind_address.c_str(), &address.sin_addr) != 1) {
            throw std::invalid_argument("bind_address must be an IPv4 literal");
        }
        if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) <
            0) {
            throw_system_error("bind");
        }
        if (::listen(listener.get(), config.backlog) < 0) {
            throw_system_error("listen");
        }

        sockaddr_in bound{};
        socklen_t bound_length = sizeof(bound);
        if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound), &bound_length) < 0) {
            throw_system_error("getsockname");
        }

        UniqueFd poller(::epoll_create1(EPOLL_CLOEXEC));
        if (poller.get() < 0) {
            throw_system_error("epoll_create1");
        }
        UniqueFd wake(::eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC));
        if (wake.get() < 0) {
            throw_system_error("eventfd");
        }
        UniqueFd timer(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC));
        if (timer.get() < 0) {
            throw_system_error("timerfd_create");
        }

        epoll_event listener_event{};
        listener_event.events = EPOLLIN;
        listener_event.data.u64 = kListenerToken;
        if (::epoll_ctl(poller.get(), EPOLL_CTL_ADD, listener.get(), &listener_event) < 0) {
            throw_system_error("epoll_ctl(listener)");
        }

        epoll_event wake_event{};
        wake_event.events = EPOLLIN;
        wake_event.data.u64 = kWakeToken;
        if (::epoll_ctl(poller.get(), EPOLL_CTL_ADD, wake.get(), &wake_event) < 0) {
            throw_system_error("epoll_ctl(eventfd)");
        }

        epoll_event timer_event{};
        timer_event.events = EPOLLIN;
        timer_event.data.u64 = kTimerToken;
        if (::epoll_ctl(poller.get(), EPOLL_CTL_ADD, timer.get(), &timer_event) < 0) {
            throw_system_error("epoll_ctl(timerfd)");
        }

        bound_port = ntohs(bound.sin_port);
        listen_fd = listener.release();
        epoll_fd = poller.release();
        wake_fd = wake.release();
        timer_fd = timer.release();
    }

    ~Impl() {
        pool.shutdown();
        cleanup_connections();
        if (listen_fd >= 0) {
            (void)::close(listen_fd);
        }
        if (epoll_fd >= 0) {
            (void)::close(epoll_fd);
        }
        if (wake_fd >= 0) {
            (void)::close(wake_fd);
        }
        if (timer_fd >= 0) {
            (void)::close(timer_fd);
        }
    }

    HttpServerConfig config;
    Router router;
    StaticFileHandler static_files;
    int listen_fd{-1};
    int epoll_fd{-1};
    int wake_fd{-1};
    int timer_fd{-1};
    std::uint16_t bound_port{0U};
    std::atomic<bool> is_running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> has_run{false};
    std::uint64_t next_token{kFirstConnectionToken};
    std::unordered_map<std::uint64_t, Connection> connections;
    std::mutex completion_mutex;
    std::deque<Completion> completions;
    std::size_t outstanding_jobs{0U};
    std::size_t persistent_connections{0U};
    std::priority_queue<DeadlineEntry, std::vector<DeadlineEntry>, DeadlineLater> deadlines;
    cppapplab::concurrency::ThreadPool pool;

    void discard_stale_deadlines() {
        while (!deadlines.empty()) {
            const DeadlineEntry& entry = deadlines.top();
            const auto found = connections.find(entry.token);
            if (found != connections.end() &&
                found->second.deadline_generation == entry.generation &&
                found->second.deadline_kind == entry.kind) {
                break;
            }
            deadlines.pop();
        }
    }

    void compact_deadlines_if_needed() {
        const std::size_t threshold = connections.size() * 4U + 64U;
        if (deadlines.size() <= threshold) {
            return;
        }

        decltype(deadlines) compacted;
        for (const auto& item : connections) {
            const Connection& connection = item.second;
            if (connection.deadline_kind != DeadlineKind::None) {
                compacted.push(DeadlineEntry{connection.deadline_at, connection.token,
                                             connection.deadline_generation,
                                             connection.deadline_kind});
            }
        }
        deadlines.swap(compacted);
    }

    void schedule_timer() {
        discard_stale_deadlines();

        itimerspec timer_spec{};
        if (!deadlines.empty()) {
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   deadlines.top().when.time_since_epoch())
                                   .count();
            if (nanoseconds <= 0) {
                nanoseconds = 1;
            }
            constexpr std::int64_t kNanosecondsPerSecond = 1000000000LL;
            timer_spec.it_value.tv_sec = static_cast<time_t>(nanoseconds / kNanosecondsPerSecond);
            timer_spec.it_value.tv_nsec = static_cast<long>(nanoseconds % kNanosecondsPerSecond);
        }

        if (::timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &timer_spec, nullptr) < 0) {
            throw_system_error("timerfd_settime");
        }
    }

    void arm_deadline(Connection& connection, DeadlineKind kind,
                      std::chrono::milliseconds timeout) {
        if (connection.deadline_kind == kind) {
            return;  // Keep the original absolute deadline; trickle traffic cannot extend it.
        }

        ++connection.deadline_generation;
        if (connection.deadline_generation == 0U) {
            ++connection.deadline_generation;
        }
        connection.deadline_kind = kind;
        connection.deadline_at = Clock::now() + timeout;
        deadlines.push(DeadlineEntry{connection.deadline_at, connection.token,
                                     connection.deadline_generation, kind});
        compact_deadlines_if_needed();
        schedule_timer();
    }

    void cancel_deadline(Connection& connection) {
        if (connection.deadline_kind == DeadlineKind::None) {
            return;
        }
        ++connection.deadline_generation;
        if (connection.deadline_generation == 0U) {
            ++connection.deadline_generation;
        }
        connection.deadline_kind = DeadlineKind::None;
        schedule_timer();
    }

    void drain_timer_fd() noexcept {
        std::uint64_t expirations = 0U;
        for (;;) {
            const ssize_t count = ::read(timer_fd, &expirations, sizeof(expirations));
            if (count == static_cast<ssize_t>(sizeof(expirations))) {
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
    }

    void expire_deadlines() {
        const auto now = Clock::now();
        while (!deadlines.empty() && deadlines.top().when <= now) {
            const DeadlineEntry entry = deadlines.top();
            deadlines.pop();

            const auto found = connections.find(entry.token);
            if (found == connections.end() ||
                found->second.deadline_generation != entry.generation ||
                found->second.deadline_kind != entry.kind) {
                continue;
            }

            Connection& connection = found->second;
            connection.deadline_kind = DeadlineKind::None;
            ++connection.deadline_generation;
            if (entry.kind == DeadlineKind::Idle) {
                close_connection(entry.token);
                continue;
            }

            connection.read_buffer.clear();
            queue_error(connection, 408, "Request Timeout", "408 Request Timeout\n");
        }
        schedule_timer();
    }

    [[nodiscard]] std::uint64_t allocate_token() {
        if (next_token == 0U || next_token == kListenerToken || next_token == kWakeToken) {
            throw std::overflow_error("HTTP connection generation exhausted");
        }
        return next_token++;
    }

    [[nodiscard]] bool update_interest(const Connection& connection) const {
        epoll_event event{};
        if (!connection.processing && connection.write_offset >= connection.write_buffer.size() &&
            !connection.peer_closed && !connection.close_after_write) {
            event.events |= EPOLLIN | EPOLLRDHUP;
        }
        if (connection.write_offset < connection.write_buffer.size()) {
            event.events |= EPOLLOUT;
        }
        event.data.u64 = connection.token;
        return ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connection.fd, &event) == 0;
    }

    void close_connection(std::uint64_t token) noexcept {
        const auto found = connections.find(token);
        if (found == connections.end()) {
            return;
        }

        const int fd = found->second.fd;
        if (found->second.deadline_kind != DeadlineKind::None) {
            ++found->second.deadline_generation;
            found->second.deadline_kind = DeadlineKind::None;
        }
        if (found->second.persistent_slot_reserved && persistent_connections > 0U) {
            --persistent_connections;
        }
        if (fd >= 0) {
            (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            (void)::close(fd);
        }
        connections.erase(found);
    }

    void cleanup_connections() noexcept {
        while (!connections.empty()) {
            close_connection(connections.begin()->first);
        }
    }

    void queue_response(Connection& connection, HttpResponse response,
                        ConnectionPolicy connection_policy = ConnectionPolicy::Close) {
        cancel_deadline(connection);
        std::string version = connection.response_version;
        if (version != "HTTP/1.0" && version != "HTTP/1.1") {
            version = "HTTP/1.1";
        }

        if (connection.peer_closed && connection.read_buffer.empty()) {
            connection_policy = ConnectionPolicy::Close;
        }
        if (response.body.size() > config.max_response_bytes) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
            connection_policy = ConnectionPolicy::Close;
        }

        std::string wire;
        try {
            wire = serialize_response(response, connection_policy, version);
        } catch (...) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
            connection_policy = ConnectionPolicy::Close;
            wire = serialize_response(response, connection_policy, version);
        }

        if (wire.size() > config.max_response_bytes) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
            connection_policy = ConnectionPolicy::Close;
            wire = serialize_response(response, connection_policy, version);
        }

        connection.write_buffer = std::move(wire);
        connection.write_offset = 0U;
        connection.processing = false;
        connection.close_after_write = connection_policy == ConnectionPolicy::Close;
        const std::uint64_t token = connection.token;
        if (!update_interest(connection)) {
            close_connection(token);
        }
    }

    void queue_error(Connection& connection, int status, const char* reason, const char* body) {
        queue_response(connection, error_response(status, reason, body));
    }

    [[nodiscard]] bool flush_write(Connection& connection) {
        const std::uint64_t token = connection.token;
        while (connection.write_offset < connection.write_buffer.size()) {
            const ssize_t sent =
                ::send(connection.fd, connection.write_buffer.data() + connection.write_offset,
                       connection.write_buffer.size() - connection.write_offset, MSG_NOSIGNAL);
            if (sent > 0) {
                connection.write_offset += static_cast<std::size_t>(sent);
                continue;
            }
            if (sent < 0 && errno == EINTR) {
                continue;
            }
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return update_interest(connection);
            }
            return false;
        }

        if (connection.close_after_write) {
            return false;
        }

        connection.write_buffer.clear();
        connection.write_offset = 0U;
        try_dispatch(connection);

        const auto current = connections.find(token);
        if (current == connections.end()) {
            return false;
        }
        if (current->second.processing ||
            current->second.write_offset < current->second.write_buffer.size()) {
            return true;
        }
        return update_interest(current->second);
    }

    void publish_completion(Completion completion) {
        {
            std::lock_guard<std::mutex> lock(completion_mutex);
            completions.push_back(std::move(completion));
        }

        const std::uint64_t one = 1U;
        for (;;) {
            const ssize_t written = ::write(wake_fd, &one, sizeof(one));
            if (written == static_cast<ssize_t>(sizeof(one)) || (written < 0 && errno == EAGAIN)) {
                return;
            }
            if (written < 0 && errno == EINTR) {
                continue;
            }
            return;
        }
    }

    void submit_request(Connection& connection, HttpRequest request,
                        ConnectionPolicy connection_policy) {
        const std::uint64_t token = connection.token;
        const int fd = connection.fd;
        connection.pending_policy = connection_policy;
        if (!request.version.empty()) {
            connection.response_version = request.version;
        }
        if (outstanding_jobs >= config.task_queue_capacity) {
            queue_error(connection, 503, "Service Unavailable", "503 Service Unavailable\n");
            return;
        }

        connection.processing = true;
        if (!update_interest(connection)) {
            close_connection(token);
            return;
        }

        ++outstanding_jobs;
        try {
            (void)pool.submit([this, fd, token, request = std::move(request)]() mutable {
                HttpResponse response;
                try {
                    response = dispatch_request(request, router, static_files);
                } catch (...) {
                    response =
                        error_response(500, "Internal Server Error", "500 Internal Server Error\n");
                }
                publish_completion(Completion{fd, token, std::move(response)});
            });
        } catch (...) {
            --outstanding_jobs;
            const auto current = connections.find(token);
            if (current != connections.end()) {
                queue_error(current->second, 503, "Service Unavailable",
                            "503 Service Unavailable\n");
            }
        }
    }

    void try_dispatch(Connection& connection) {
        if (connection.processing || connection.close_after_write ||
            connection.write_offset < connection.write_buffer.size()) {
            return;
        }

        const std::uint64_t token = connection.token;
        if (connection.read_buffer.empty()) {
            if (connection.peer_closed) {
                close_connection(token);
            } else {
                arm_deadline(connection, DeadlineKind::Idle, config.idle_timeout);
            }
            return;
        }

        ParseResult parsed = HttpRequestParser{}.parse(connection.read_buffer);
        if (!parsed.request.version.empty()) {
            connection.response_version = parsed.request.version;
        }
        if (parsed.status == ParseStatus::NeedMoreData) {
            if (connection.peer_closed) {
                queue_error(connection, 400, "Bad Request", "400 Bad Request\n");
            } else if (connection.read_buffer.size() > config.max_request_bytes) {
                queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
            } else if (connection.read_buffer.find("\r\n\r\n") == std::string::npos) {
                arm_deadline(connection, DeadlineKind::Header, config.header_timeout);
            } else {
                arm_deadline(connection, DeadlineKind::Body, config.body_timeout);
            }
            return;
        }
        if (parsed.status == ParseStatus::Error) {
            if (is_too_large_error(parsed.error)) {
                queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
            } else {
                queue_error(connection, 400, "Bad Request", "400 Bad Request\n");
            }
            return;
        }
        if (parsed.consumed > config.max_request_bytes) {
            queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
            return;
        }

        cancel_deadline(connection);
        ++connection.requests_started;
        ConnectionPolicy connection_policy = request_connection_policy(parsed.request);
        const bool request_limit_reached =
            connection.requests_started >= config.max_requests_per_connection;
        if (request_limit_reached) {
            connection_policy = ConnectionPolicy::Close;
        }
        if (connection_policy == ConnectionPolicy::KeepAlive &&
            !connection.persistent_slot_reserved && !request_limit_reached) {
            if (persistent_connections >= config.max_persistent_connections) {
                connection_policy = ConnectionPolicy::Close;
            } else {
                connection.persistent_slot_reserved = true;
                ++persistent_connections;
            }
        }
        connection.read_buffer.erase(0U, parsed.consumed);
        if (connection.peer_closed && connection.read_buffer.empty()) {
            connection_policy = ConnectionPolicy::Close;
        }
        submit_request(connection, std::move(parsed.request), connection_policy);
    }

    [[nodiscard]] bool read_client(Connection& connection, bool peer_hangup) {
        const std::uint64_t token = connection.token;
        if (peer_hangup) {
            connection.peer_closed = true;
        }
        char buffer[8192];

        for (;;) {
            if (connection.read_buffer.size() > config.max_request_bytes) {
                if (connection.peer_closed) {
                    queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
                    return connections.find(token) != connections.end();
                }
                try_dispatch(connection);
                const auto current = connections.find(token);
                if (current == connections.end()) {
                    return false;
                }
                if (current->second.processing ||
                    current->second.write_offset < current->second.write_buffer.size()) {
                    return true;
                }
                queue_error(current->second, 413, "Content Too Large", "413 Content Too Large\n");
                return connections.find(token) != connections.end();
            }

            const std::size_t remaining = config.max_request_bytes - connection.read_buffer.size();
            const std::size_t receive_capacity =
                remaining >= sizeof(buffer) ? sizeof(buffer) : remaining + 1U;
            const ssize_t received = ::recv(connection.fd, buffer, receive_capacity, 0);

            if (received > 0) {
                connection.read_buffer.append(buffer, static_cast<std::size_t>(received));
                if (!connection.peer_closed) {
                    try_dispatch(connection);
                    const auto current = connections.find(token);
                    if (current == connections.end()) {
                        return false;
                    }
                    if (current->second.processing ||
                        current->second.write_offset < current->second.write_buffer.size()) {
                        return true;
                    }
                }
                continue;
            }
            if (received == 0) {
                connection.peer_closed = true;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }

        try_dispatch(connection);
        return connections.find(token) != connections.end();
    }

    void accept_clients() {
        for (;;) {
            const int accepted =
                ::accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (accepted < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                throw_system_error("accept4");
            }

            UniqueFd client(accepted);
            if (connections.size() >= config.max_connections) {
                const std::string busy = serialize_response(
                    error_response(503, "Service Unavailable", "503 Service Unavailable\n"));
                send_best_effort(client.get(), busy);
                continue;
            }

            const std::uint64_t token = allocate_token();
            Connection connection;
            connection.fd = client.get();
            connection.token = token;
            const auto inserted = connections.emplace(token, std::move(connection));
            if (!inserted.second) {
                continue;
            }

            epoll_event event{};
            event.events = EPOLLIN | EPOLLRDHUP;
            event.data.u64 = token;
            if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.get(), &event) < 0) {
                connections.erase(inserted.first);
                continue;
            }
            (void)client.release();
            try {
                arm_deadline(inserted.first->second, DeadlineKind::Idle, config.idle_timeout);
            } catch (...) {
                close_connection(token);
                throw;
            }
        }
    }

    void drain_wake_fd() noexcept {
        std::uint64_t value = 0U;
        for (;;) {
            const ssize_t count = ::read(wake_fd, &value, sizeof(value));
            if (count == static_cast<ssize_t>(sizeof(value))) {
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
    }

    void drain_completions() {
        std::deque<Completion> ready;
        {
            std::lock_guard<std::mutex> lock(completion_mutex);
            ready.swap(completions);
        }

        for (auto& completion : ready) {
            if (outstanding_jobs > 0U) {
                --outstanding_jobs;
            }
            const auto found = connections.find(completion.token);
            if (found == connections.end() || found->second.fd != completion.fd) {
                continue;
            }
            queue_response(found->second, std::move(completion.response),
                           found->second.pending_policy);
        }
    }

    void run_loop() {
        epoll_event events[64]{};
        while (!stop_requested.load()) {
            const int count = ::epoll_wait(epoll_fd, events, 64, -1);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw_system_error("epoll_wait");
            }

            for (int index = 0; index < count; ++index) {
                if (stop_requested.load()) {
                    break;
                }
                const std::uint64_t token = events[index].data.u64;
                const std::uint32_t flags = events[index].events;

                if (token == kWakeToken) {
                    drain_wake_fd();
                    drain_completions();
                    continue;
                }
                if (token == kTimerToken) {
                    drain_timer_fd();
                    expire_deadlines();
                    continue;
                }
                if (token == kListenerToken) {
                    accept_clients();
                    continue;
                }

                auto found = connections.find(token);
                if (found == connections.end()) {
                    continue;
                }

                bool keep = true;
                if ((flags & (EPOLLERR | EPOLLHUP)) != 0U) {
                    keep = false;
                } else if ((flags & (EPOLLIN | EPOLLRDHUP)) != 0U) {
                    keep = read_client(found->second, (flags & EPOLLRDHUP) != 0U);
                }

                found = connections.find(token);
                if (keep && found != connections.end() && (flags & EPOLLOUT) != 0U) {
                    keep = flush_write(found->second);
                }
                if (!keep) {
                    close_connection(token);
                }
            }
        }
        cleanup_connections();
    }

    void request_stop() noexcept {
        stop_requested.store(true);
        if (wake_fd < 0) {
            return;
        }

        const std::uint64_t one = 1U;
        for (;;) {
            const ssize_t written = ::write(wake_fd, &one, sizeof(one));
            if (written == static_cast<ssize_t>(sizeof(one)) || (written < 0 && errno == EAGAIN)) {
                return;
            }
            if (written < 0 && errno == EINTR) {
                continue;
            }
            return;
        }
    }
};

HttpServer::HttpServer(HttpServerConfig config, Router router)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(router))) {}

HttpServer::~HttpServer() {
    if (impl_) {
        impl_->request_stop();
    }
}

std::uint16_t HttpServer::port() const noexcept {
    return impl_->bound_port;
}

bool HttpServer::running() const noexcept {
    return impl_->is_running.load();
}

void HttpServer::run() {
    bool expected = false;
    if (!impl_->has_run.compare_exchange_strong(expected, true)) {
        throw std::logic_error("HttpServer::run may only be called once");
    }

    impl_->is_running.store(true);
    try {
        impl_->run_loop();
    } catch (...) {
        impl_->cleanup_connections();
        impl_->is_running.store(false);
        throw;
    }
    impl_->is_running.store(false);
}

void HttpServer::request_stop() noexcept {
    impl_->request_stop();
}

}  // namespace cppapplab::http
