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
#include <unistd.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace cppapplab::http {
namespace {

constexpr std::uint64_t kListenerToken = 1U;
constexpr std::uint64_t kWakeToken = 2U;
constexpr std::uint64_t kFirstConnectionToken = 3U;
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
    if (config.backlog <= 0 || config.worker_count == 0U || config.task_queue_capacity == 0U ||
        config.max_connections == 0U || config.max_request_bytes == 0U ||
        config.max_response_bytes < 512U) {
        throw std::invalid_argument("HTTP server limits must be positive");
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
    struct Connection {
        int fd{-1};
        std::uint64_t token{0U};
        std::string read_buffer;
        std::string write_buffer;
        std::size_t write_offset{0U};
        bool processing{false};
        bool peer_closed{false};
        bool close_after_write{false};
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

        bound_port = ntohs(bound.sin_port);
        listen_fd = listener.release();
        epoll_fd = poller.release();
        wake_fd = wake.release();
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
    }

    HttpServerConfig config;
    Router router;
    StaticFileHandler static_files;
    int listen_fd{-1};
    int epoll_fd{-1};
    int wake_fd{-1};
    std::uint16_t bound_port{0U};
    std::atomic<bool> is_running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> has_run{false};
    std::uint64_t next_token{kFirstConnectionToken};
    std::unordered_map<std::uint64_t, Connection> connections;
    std::mutex completion_mutex;
    std::deque<Completion> completions;
    std::size_t outstanding_jobs{0U};
    cppapplab::concurrency::ThreadPool pool;

    [[nodiscard]] std::uint64_t allocate_token() {
        if (next_token == 0U || next_token == kListenerToken || next_token == kWakeToken) {
            throw std::overflow_error("HTTP connection generation exhausted");
        }
        return next_token++;
    }

    [[nodiscard]] bool update_interest(const Connection& connection) const {
        epoll_event event{};
        if (!connection.processing && !connection.peer_closed && !connection.close_after_write) {
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

    void queue_response(Connection& connection, HttpResponse response) {
        if (response.body.size() > config.max_response_bytes) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
        }

        std::string wire;
        try {
            wire = serialize_response(response);
        } catch (...) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
            wire = serialize_response(response);
        }

        if (wire.size() > config.max_response_bytes) {
            response = error_response(500, "Internal Server Error", "500 Internal Server Error\n");
            wire = serialize_response(response);
        }

        connection.write_buffer = std::move(wire);
        connection.write_offset = 0U;
        connection.processing = false;
        connection.close_after_write = true;
        const std::uint64_t token = connection.token;
        if (!update_interest(connection)) {
            close_connection(token);
        }
    }

    void queue_error(Connection& connection, int status, const char* reason, const char* body) {
        queue_response(connection, error_response(status, reason, body));
    }

    [[nodiscard]] bool flush_write(Connection& connection) {
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
        // TODO(student): finish the response, then reuse or close the connection by policy.
        return false;  // D5 always emits Connection: close.
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

    void submit_request(Connection& connection, HttpRequest request) {
        const std::uint64_t token = connection.token;
        const int fd = connection.fd;
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
        if (connection.processing || connection.close_after_write) {
            return;
        }

        const std::uint64_t token = connection.token;
        if (connection.read_buffer.empty()) {
            if (connection.peer_closed) {
                close_connection(token);
            }
            return;
        }

        ParseResult parsed = HttpRequestParser{}.parse(connection.read_buffer);
        if (parsed.status == ParseStatus::NeedMoreData) {
            if (connection.peer_closed) {
                queue_error(connection, 400, "Bad Request", "400 Bad Request\n");
            } else if (connection.read_buffer.size() > config.max_request_bytes) {
                queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
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

        submit_request(connection, std::move(parsed.request));
    }

    [[nodiscard]] bool read_client(Connection& connection, bool peer_hangup) {
        const std::uint64_t token = connection.token;
        char buffer[8192];

        for (;;) {
            const std::size_t remaining = config.max_request_bytes - connection.read_buffer.size();
            const std::size_t receive_capacity =
                remaining >= sizeof(buffer) ? sizeof(buffer) : remaining + 1U;
            const ssize_t received = ::recv(connection.fd, buffer, receive_capacity, 0);

            if (received > 0) {
                const std::size_t received_bytes = static_cast<std::size_t>(received);
                if (received_bytes > remaining) {
                    queue_error(connection, 413, "Content Too Large", "413 Content Too Large\n");
                    return connections.find(token) != connections.end();
                }
                connection.read_buffer.append(buffer, received_bytes);
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

        if (peer_hangup) {
            connection.peer_closed = true;
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
            queue_response(found->second, std::move(completion.response));
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
