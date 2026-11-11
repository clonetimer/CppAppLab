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
#include <iostream>
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
        !valid_timeout(config.header_timeout) || !valid_timeout(config.body_timeout) ||
        !valid_timeout(config.shutdown_timeout)) {
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
        Clock::time_point request_started_at{};
        std::string request_method;
        std::string request_target;
        int response_status_code{0};
    };

    struct Completion {
        int fd{-1};
        std::uint64_t token{0U};
        HttpResponse response;
        bool handler_failed{false};
    };

    struct AtomicMetrics {
        std::atomic<std::uint64_t> accepted_connections{0U};
        std::atomic<std::uint64_t> rejected_connections{0U};
        std::atomic<std::uint64_t> active_connections{0U};
        std::atomic<std::uint64_t> closed_connections{0U};
        std::atomic<std::uint64_t> persistent_connections{0U};
        std::atomic<std::uint64_t> requests_received{0U};
        std::atomic<std::uint64_t> responses_completed{0U};
        std::atomic<std::uint64_t> response_bytes{0U};
        std::atomic<std::uint64_t> client_errors{0U};
        std::atomic<std::uint64_t> server_errors{0U};
        std::atomic<std::uint64_t> request_timeouts{0U};
        std::atomic<std::uint64_t> idle_timeouts{0U};
        std::atomic<std::uint64_t> handler_failures{0U};
        std::atomic<std::uint64_t> queue_rejections{0U};
        std::atomic<std::uint64_t> active_jobs{0U};
        std::atomic<std::uint64_t> graceful_shutdowns{0U};
        std::atomic<std::uint64_t> forced_shutdown_connections{0U};
        std::atomic<std::uint64_t> log_sink_failures{0U};
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
    AtomicMetrics metric_counters;
    bool shutting_down{false};
    bool shutdown_deadline_expired{false};
    Clock::time_point shutdown_deadline{};
    cppapplab::concurrency::ThreadPool pool;

    [[nodiscard]] static std::string json_quote(std::string_view value) {
        constexpr char kHex[] = "0123456789abcdef";
        std::string output;
        output.reserve(value.size() + 2U);
        output.push_back('"');
        for (const unsigned char character : value) {
            switch (character) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (character < 0x20U) {
                        output += "\\u00";
                        output.push_back(kHex[(character >> 4U) & 0x0fU]);
                        output.push_back(kHex[character & 0x0fU]);
                    } else {
                        output.push_back(static_cast<char>(character));
                    }
                    break;
            }
        }
        output.push_back('"');
        return output;
    }

    [[nodiscard]] static std::string log_prefix(std::string_view level, std::string_view event) {
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
        return "{\"timestamp_ms\":" + std::to_string(timestamp) +
               ",\"level\":" + json_quote(level) + ",\"event\":" + json_quote(event);
    }

    void emit_log(const std::string& line, bool access_log) noexcept {
        if ((access_log && !config.access_log_enabled) ||
            (!access_log && !config.error_log_enabled)) {
            return;
        }

        try {
            if (config.log_sink) {
                config.log_sink(line);
            } else {
                static std::mutex output_mutex;
                const std::lock_guard<std::mutex> lock(output_mutex);
                std::clog << line << '\n';
            }
        } catch (...) {
            metric_counters.log_sink_failures.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    void log_server_event(std::string_view event, std::string_view detail = {},
                          std::uint64_t count = 0U) {
        std::string line = log_prefix("info", event);
        if (!detail.empty()) {
            line += ",\"detail\":" + json_quote(detail);
        }
        if (count != 0U) {
            line += ",\"count\":" + std::to_string(count);
        }
        line.push_back('}');
        emit_log(line, false);
    }

    void log_error_event(std::string_view event, std::string_view detail, int status_code = 0,
                         std::uint64_t connection_id = 0U) {
        std::string line = log_prefix("error", event);
        if (status_code != 0) {
            line += ",\"status\":" + std::to_string(status_code);
        }
        if (connection_id != 0U) {
            line += ",\"connection_id\":" + std::to_string(connection_id);
        }
        if (!detail.empty()) {
            line += ",\"detail\":" + json_quote(detail);
        }
        line.push_back('}');
        emit_log(line, false);
    }

    void log_access(const Connection& connection, std::size_t response_bytes,
                    std::uint64_t duration_ms) {
        std::string line = log_prefix("info", "http_access");
        line += ",\"connection_id\":" + std::to_string(connection.token);
        line += ",\"method\":" + json_quote(connection.request_method);
        const std::size_t query_position = connection.request_target.find('?');
        const std::string_view safe_target(connection.request_target.data(),
                                           query_position == std::string::npos
                                               ? connection.request_target.size()
                                               : query_position);
        line += ",\"target\":" + json_quote(safe_target);
        line += std::string(",\"query_present\":") +
                (query_position == std::string::npos ? "false" : "true");
        line += ",\"status\":" + std::to_string(connection.response_status_code);
        line += ",\"duration_ms\":" + std::to_string(duration_ms);
        line += ",\"response_bytes\":" + std::to_string(response_bytes);
        line += ",\"connection\":";
        line += json_quote(connection.close_after_write ? "close" : "keep-alive");
        line.push_back('}');
        emit_log(line, true);
    }

    [[nodiscard]] HttpServerMetrics snapshot_metrics() const noexcept {
        HttpServerMetrics snapshot;
        snapshot.accepted_connections = metric_counters.accepted_connections.load();
        snapshot.rejected_connections = metric_counters.rejected_connections.load();
        snapshot.active_connections = metric_counters.active_connections.load();
        snapshot.closed_connections = metric_counters.closed_connections.load();
        snapshot.persistent_connections = metric_counters.persistent_connections.load();
        snapshot.requests_received = metric_counters.requests_received.load();
        snapshot.responses_completed = metric_counters.responses_completed.load();
        snapshot.response_bytes = metric_counters.response_bytes.load();
        snapshot.client_errors = metric_counters.client_errors.load();
        snapshot.server_errors = metric_counters.server_errors.load();
        snapshot.request_timeouts = metric_counters.request_timeouts.load();
        snapshot.idle_timeouts = metric_counters.idle_timeouts.load();
        snapshot.handler_failures = metric_counters.handler_failures.load();
        snapshot.queue_rejections = metric_counters.queue_rejections.load();
        snapshot.active_jobs = metric_counters.active_jobs.load();
        snapshot.graceful_shutdowns = metric_counters.graceful_shutdowns.load();
        snapshot.forced_shutdown_connections = metric_counters.forced_shutdown_connections.load();
        snapshot.log_sink_failures = metric_counters.log_sink_failures.load();
        return snapshot;
    }

    void record_response(Connection& connection) {
        const std::size_t bytes = connection.write_buffer.size();
        const auto elapsed = connection.request_started_at == Clock::time_point{}
                                 ? std::chrono::milliseconds::zero()
                                 : std::chrono::duration_cast<std::chrono::milliseconds>(
                                       Clock::now() - connection.request_started_at);
        const auto duration_ms =
            static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed.count()));

        metric_counters.responses_completed.fetch_add(1U, std::memory_order_relaxed);
        metric_counters.response_bytes.fetch_add(static_cast<std::uint64_t>(bytes),
                                                 std::memory_order_relaxed);
        if (connection.response_status_code >= 400 && connection.response_status_code < 500) {
            metric_counters.client_errors.fetch_add(1U, std::memory_order_relaxed);
        } else if (connection.response_status_code >= 500 &&
                   connection.response_status_code < 600) {
            metric_counters.server_errors.fetch_add(1U, std::memory_order_relaxed);
        }

        log_access(connection, bytes, duration_ms);
        connection.request_method.clear();
        connection.request_target.clear();
        connection.response_status_code = 0;
        connection.request_started_at =
            connection.read_buffer.empty() ? Clock::time_point{} : Clock::now();
    }

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

        bool has_deadline = false;
        Clock::time_point next_deadline{};
        if (!deadlines.empty()) {
            next_deadline = deadlines.top().when;
            has_deadline = true;
        }
        if (shutting_down && !shutdown_deadline_expired &&
            (!has_deadline || shutdown_deadline < next_deadline)) {
            next_deadline = shutdown_deadline;
            has_deadline = true;
        }

        itimerspec timer_spec{};
        if (has_deadline) {
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   next_deadline.time_since_epoch())
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
                metric_counters.idle_timeouts.fetch_add(1U, std::memory_order_relaxed);
                log_error_event("idle_timeout", "idle_deadline_expired", 0, entry.token);
                close_connection(entry.token);
                continue;
            }

            metric_counters.request_timeouts.fetch_add(1U, std::memory_order_relaxed);
            log_error_event(entry.kind == DeadlineKind::Header ? "header_timeout" : "body_timeout",
                            "request_deadline_expired", 408, entry.token);
            connection.read_buffer.clear();
            queue_error(connection, 408, "Request Timeout", "408 Request Timeout\n");
        }
        if (shutting_down && !shutdown_deadline_expired && shutdown_deadline <= now) {
            shutdown_deadline_expired = true;
            const auto forced_connections = static_cast<std::uint64_t>(connections.size());
            metric_counters.forced_shutdown_connections.fetch_add(forced_connections,
                                                                  std::memory_order_relaxed);
            log_server_event("shutdown_deadline_expired", "connection_drain_timeout",
                             forced_connections);
            cleanup_connections();
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
            metric_counters.persistent_connections.store(persistent_connections,
                                                         std::memory_order_relaxed);
        }
        if (fd >= 0) {
            (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            (void)::close(fd);
        }
        connections.erase(found);
        metric_counters.closed_connections.fetch_add(1U, std::memory_order_relaxed);
        metric_counters.active_connections.store(connections.size(), std::memory_order_relaxed);
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

        connection.response_status_code = response.status_code;
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
        log_error_event("http_error", reason, status, connection.token);
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

        if (!connection.write_buffer.empty()) {
            record_response(connection);
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
            metric_counters.queue_rejections.fetch_add(1U, std::memory_order_relaxed);
            log_error_event("worker_queue_rejected", "task_queue_capacity", 503, token);
            queue_error(connection, 503, "Service Unavailable", "503 Service Unavailable\n");
            return;
        }

        connection.processing = true;
        if (!update_interest(connection)) {
            close_connection(token);
            return;
        }

        ++outstanding_jobs;
        metric_counters.active_jobs.fetch_add(1U, std::memory_order_relaxed);
        try {
            (void)pool.submit([this, fd, token, request = std::move(request)]() mutable {
                HttpResponse response;
                bool handler_failed = false;
                try {
                    response = dispatch_request(request, router, static_files);
                } catch (...) {
                    handler_failed = true;
                    response =
                        error_response(500, "Internal Server Error", "500 Internal Server Error\n");
                }
                publish_completion(Completion{fd, token, std::move(response), handler_failed});
            });
        } catch (...) {
            --outstanding_jobs;
            metric_counters.active_jobs.fetch_sub(1U, std::memory_order_relaxed);
            metric_counters.queue_rejections.fetch_add(1U, std::memory_order_relaxed);
            log_error_event("worker_queue_rejected", "thread_pool_submit_failed", 503, token);
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

        if (connection.request_started_at == Clock::time_point{}) {
            connection.request_started_at = Clock::now();
        }
        ParseResult parsed = HttpRequestParser{}.parse(connection.read_buffer);
        if (!parsed.request.method.empty()) {
            connection.request_method = parsed.request.method;
            connection.request_target = parsed.request.target;
        }
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
        metric_counters.requests_received.fetch_add(1U, std::memory_order_relaxed);
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
                metric_counters.persistent_connections.store(persistent_connections,
                                                             std::memory_order_relaxed);
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
                if (connection.read_buffer.empty()) {
                    connection.request_started_at = Clock::now();
                }
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
            if (stop_requested.load()) {
                return;
            }
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
                metric_counters.rejected_connections.fetch_add(1U, std::memory_order_relaxed);
                log_error_event("connection_rejected", "max_connections", 503);
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
            metric_counters.accepted_connections.fetch_add(1U, std::memory_order_relaxed);
            metric_counters.active_connections.store(connections.size(), std::memory_order_relaxed);
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
                metric_counters.active_jobs.fetch_sub(1U, std::memory_order_relaxed);
            }
            if (completion.handler_failed) {
                metric_counters.handler_failures.fetch_add(1U, std::memory_order_relaxed);
                log_error_event("handler_failure", "handler_exception", 500, completion.token);
            }
            const auto found = connections.find(completion.token);
            if (found == connections.end() || found->second.fd != completion.fd) {
                continue;
            }
            queue_response(found->second, std::move(completion.response),
                           found->second.pending_policy);
        }
    }

    void begin_shutdown() {
        if (shutting_down) {
            return;
        }
        shutting_down = true;
        shutdown_deadline = Clock::now() + config.shutdown_timeout;
        metric_counters.graceful_shutdowns.fetch_add(1U, std::memory_order_relaxed);
        log_server_event("server_shutdown_started", "request_stop",
                         static_cast<std::uint64_t>(connections.size()));

        if (listen_fd >= 0) {
            (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, listen_fd, nullptr);
            (void)::close(listen_fd);
            listen_fd = -1;
        }

        std::vector<std::uint64_t> tokens;
        tokens.reserve(connections.size());
        for (const auto& item : connections) {
            tokens.push_back(item.first);
        }

        for (const std::uint64_t token : tokens) {
            const auto found = connections.find(token);
            if (found == connections.end()) {
                continue;
            }

            Connection& connection = found->second;
            if (connection.processing) {
                // The worker completion will be serialized with Connection: close.
                connection.pending_policy = ConnectionPolicy::Close;
                continue;
            }

            if (connection.write_offset < connection.write_buffer.size()) {
                connection.close_after_write = true;
                if (connection.write_offset == 0U) {
                    const std::string keep_alive = "Connection: keep-alive\r\n";
                    const std::size_t position = connection.write_buffer.find(keep_alive);
                    if (position != std::string::npos) {
                        connection.write_buffer.replace(position, keep_alive.size(),
                                                        "Connection: close\r\n");
                    }
                }
                if (!update_interest(connection)) {
                    close_connection(token);
                }
                continue;
            }

            if (!connection.read_buffer.empty()) {
                connection.read_buffer.clear();
                queue_error(connection, 503, "Service Unavailable", "503 Service Unavailable\n");
                continue;
            }

            close_connection(token);
        }

        schedule_timer();
    }

    [[nodiscard]] bool shutdown_complete() const noexcept {
        return shutting_down && connections.empty() && outstanding_jobs == 0U;
    }

    void run_loop() {
        epoll_event events[64]{};
        for (;;) {
            if (stop_requested.load() && !shutting_down) {
                begin_shutdown();
            }
            if (shutdown_complete()) {
                break;
            }

            const int count = ::epoll_wait(epoll_fd, events, 64, -1);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw_system_error("epoll_wait");
            }

            for (int index = 0; index < count; ++index) {
                if (stop_requested.load() && !shutting_down) {
                    begin_shutdown();
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
                    if (!shutting_down) {
                        accept_clients();
                    }
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

HttpServerMetrics HttpServer::metrics() const noexcept {
    return impl_->snapshot_metrics();
}

void HttpServer::run() {
    bool expected = false;
    if (!impl_->has_run.compare_exchange_strong(expected, true)) {
        throw std::logic_error("HttpServer::run may only be called once");
    }

    impl_->is_running.store(true);
    impl_->log_server_event("server_started", impl_->config.bind_address, impl_->bound_port);
    try {
        impl_->run_loop();
    } catch (...) {
        impl_->log_error_event("server_loop_failure", "reactor_exception");
        impl_->cleanup_connections();
        impl_->is_running.store(false);
        throw;
    }
    impl_->log_server_event("server_stopped");
    impl_->is_running.store(false);
}

void HttpServer::request_stop() noexcept {
    impl_->request_stop();
}

}  // namespace cppapplab::http
