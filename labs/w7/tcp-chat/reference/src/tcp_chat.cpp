#include "tcp_chat.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cppapplab::net {
namespace {

class UniqueFd {
   public:
    explicit UniqueFd(int value = -1) noexcept : value_(value) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    ~UniqueFd() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
    [[nodiscard]] int get() const noexcept {
        return value_;
    }
    [[nodiscard]] int release() noexcept {
        const int value = value_;
        value_ = -1;
        return value;
    }

   private:
    int value_;
};

[[noreturn]] void throw_system_error(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}

[[nodiscard]] bool valid_nickname(const std::string& name) {
    if (name.empty() || name.size() > 24U) {
        return false;
    }
    const auto is_allowed = [](unsigned char character) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool uppercase = character >= 'A' && character <= 'Z';
        const bool digit = character >= '0' && character <= '9';
        return lowercase || uppercase || digit || character == '_' || character == '-';
    };
    return std::all_of(name.begin(), name.end(), is_allowed);
}

}  // namespace

struct ChatServer::Impl {
    struct Client {
        std::string nickname;
        std::string input;
        std::string output;
        std::size_t output_offset{0};
    };

    explicit Impl(ChatServerConfig server_config) : config(std::move(server_config)) {}

    ChatServerConfig config;
    int listen_fd{-1};
    int epoll_fd{-1};
    int stop_fd{-1};
    std::uint16_t bound_port{0};
    std::atomic<bool> is_running{false};
    std::atomic<bool> stop_requested{false};
    bool has_run{false};
    std::unordered_map<int, Client> clients;

    [[nodiscard]] bool update_interest(int fd) {
        const auto found = clients.find(fd);
        if (found == clients.end()) {
            return false;
        }
        std::uint32_t events = EPOLLIN | EPOLLRDHUP;
        if (found->second.output_offset < found->second.output.size()) {
            events |= EPOLLOUT;
        }
        epoll_event event{};
        event.events = events;
        event.data.fd = fd;
        return ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == 0;
    }

    [[nodiscard]] bool flush_client(int fd) {
        auto found = clients.find(fd);
        if (found == clients.end()) {
            return false;
        }
        Client& client = found->second;
        while (client.output_offset < client.output.size()) {
            const ssize_t sent = ::send(fd, client.output.data() + client.output_offset,
                                        client.output.size() - client.output_offset, MSG_NOSIGNAL);
            if (sent > 0) {
                client.output_offset += static_cast<std::size_t>(sent);
                continue;
            }
            if (sent < 0 && errno == EINTR) {
                continue;
            }
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return update_interest(fd);
            }
            return false;
        }
        client.output.clear();
        client.output_offset = 0U;
        return update_interest(fd);
    }

    [[nodiscard]] bool queue_output(int fd, const std::string& message) {
        auto found = clients.find(fd);
        if (found == clients.end()) {
            return false;
        }
        Client& client = found->second;
        const std::size_t pending = client.output.size() - client.output_offset;
        if (message.size() > config.max_pending_bytes ||
            pending > config.max_pending_bytes - message.size()) {
            return false;
        }
        if (client.output_offset != 0U) {
            client.output.erase(0U, client.output_offset);
            client.output_offset = 0U;
        }
        client.output += message;
        return flush_client(fd);
    }

    void broadcast(const std::string& message, int excluded_fd) {
        std::vector<int> slow_clients;
        slow_clients.reserve(clients.size());
        for (const auto& entry : clients) {
            if (entry.first == excluded_fd || entry.second.nickname.empty()) {
                continue;
            }
            if (!queue_output(entry.first, message)) {
                slow_clients.push_back(entry.first);
            }
        }
        for (const int fd : slow_clients) {
            disconnect_client(fd, true);
        }
    }

    void disconnect_client(int fd, bool announce) {
        const auto found = clients.find(fd);
        if (found == clients.end()) {
            return;
        }
        const std::string nickname = found->second.nickname;
        (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        (void)::close(fd);
        clients.erase(found);
        if (announce && !nickname.empty()) {
            broadcast("* " + nickname + " left\n", -1);
        }
    }

    [[nodiscard]] bool process_line(int fd, std::string line) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto found = clients.find(fd);
        if (found == clients.end()) {
            return false;
        }

        if (line == "QUIT") {
            return false;
        }
        if (line.rfind("NICK ", 0U) == 0U) {
            const std::string nickname = line.substr(5U);
            if (!found->second.nickname.empty()) {
                return queue_output(fd, "ERR already registered\n");
            }
            if (!valid_nickname(nickname)) {
                return queue_output(fd, "ERR invalid nickname\n");
            }
            const auto has_nickname = [&nickname](const auto& entry) {
                return entry.second.nickname == nickname;
            };
            const bool duplicate = std::any_of(clients.begin(), clients.end(), has_nickname);
            if (duplicate) {
                return queue_output(fd, "ERR nickname unavailable\n");
            }
            found->second.nickname = nickname;
            if (!queue_output(fd, "OK NICK " + nickname + "\n")) {
                return false;
            }
            broadcast("* " + nickname + " joined\n", fd);
            return clients.find(fd) != clients.end();
        }
        if (line.rfind("MSG ", 0U) == 0U) {
            if (found->second.nickname.empty()) {
                return queue_output(fd, "ERR register first\n");
            }
            const std::string message = line.substr(4U);
            if (message.empty()) {
                return queue_output(fd, "ERR empty message\n");
            }
            broadcast(found->second.nickname + ": " + message + "\n", fd);
            return clients.find(fd) != clients.end();
        }
        return queue_output(fd, "ERR unknown command\n");
    }

    [[nodiscard]] bool read_client(int fd) {
        char buffer[4096];
        for (;;) {
            const ssize_t received = ::recv(fd, buffer, sizeof(buffer), 0);
            if (received == 0) {
                return false;
            }
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return true;
                }
                return false;
            }

            auto found = clients.find(fd);
            if (found == clients.end()) {
                return false;
            }
            found->second.input.append(buffer, static_cast<std::size_t>(received));

            for (;;) {
                found = clients.find(fd);
                if (found == clients.end()) {
                    return false;
                }
                const std::size_t newline = found->second.input.find('\n');
                if (newline == std::string::npos) {
                    if (found->second.input.size() > config.max_line_bytes) {
                        return false;
                    }
                    break;
                }
                if (newline > config.max_line_bytes) {
                    return false;
                }
                std::string line = found->second.input.substr(0U, newline);
                found->second.input.erase(0U, newline + 1U);
                if (!process_line(fd, std::move(line))) {
                    return false;
                }
            }
        }
    }

    void accept_clients() {
        for (;;) {
            const int fd = ::accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                throw_system_error("accept4");
            }
            if (clients.size() >= config.max_clients) {
                const std::string full = "ERR server full\n";
                (void)::send(fd, full.data(), full.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
                (void)::close(fd);
                continue;
            }

            epoll_event event{};
            event.events = EPOLLIN | EPOLLRDHUP;
            event.data.fd = fd;
            if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
                (void)::close(fd);
                continue;
            }
            clients.emplace(fd, Client{});
        }
    }

    void cleanup_clients() noexcept {
        for (const auto& entry : clients) {
            (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, entry.first, nullptr);
            (void)::close(entry.first);
        }
        clients.clear();
    }
};

ChatServer::ChatServer(ChatServerConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {
    if (impl_->config.backlog <= 0 || impl_->config.max_clients == 0U ||
        impl_->config.max_line_bytes == 0U || impl_->config.max_pending_bytes == 0U) {
        throw std::invalid_argument("chat server limits and backlog must be positive");
    }

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
    address.sin_port = htons(impl_->config.port);
    if (::inet_pton(AF_INET, impl_->config.bind_address.c_str(), &address.sin_addr) != 1) {
        throw std::invalid_argument("bind address must be a valid IPv4 literal");
    }
    if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throw_system_error("bind");
    }
    if (::listen(listener.get(), impl_->config.backlog) < 0) {
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
    UniqueFd stop(::eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC));
    if (stop.get() < 0) {
        throw_system_error("eventfd");
    }

    epoll_event listener_event{};
    listener_event.events = EPOLLIN;
    listener_event.data.fd = listener.get();
    if (::epoll_ctl(poller.get(), EPOLL_CTL_ADD, listener.get(), &listener_event) < 0) {
        throw_system_error("epoll_ctl(listener)");
    }
    epoll_event stop_event{};
    stop_event.events = EPOLLIN;
    stop_event.data.fd = stop.get();
    if (::epoll_ctl(poller.get(), EPOLL_CTL_ADD, stop.get(), &stop_event) < 0) {
        throw_system_error("epoll_ctl(eventfd)");
    }

    impl_->bound_port = ntohs(bound.sin_port);
    impl_->listen_fd = listener.release();
    impl_->epoll_fd = poller.release();
    impl_->stop_fd = stop.release();
}

ChatServer::~ChatServer() {
    request_stop();
    impl_->cleanup_clients();
    if (impl_->listen_fd >= 0) {
        (void)::close(impl_->listen_fd);
    }
    if (impl_->epoll_fd >= 0) {
        (void)::close(impl_->epoll_fd);
    }
    if (impl_->stop_fd >= 0) {
        (void)::close(impl_->stop_fd);
    }
}

std::uint16_t ChatServer::port() const noexcept {
    return impl_->bound_port;
}

bool ChatServer::running() const noexcept {
    return impl_->is_running.load();
}

void ChatServer::run() {
    if (impl_->has_run) {
        throw std::logic_error("ChatServer::run may only be called once");
    }
    impl_->has_run = true;
    impl_->is_running.store(true);

    try {
        epoll_event events[64]{};
        while (!impl_->stop_requested.load()) {
            const int count = ::epoll_wait(impl_->epoll_fd, events, 64, -1);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw_system_error("epoll_wait");
            }
            for (int index = 0; index < count; ++index) {
                const int fd = events[index].data.fd;
                const std::uint32_t flags = events[index].events;
                if (fd == impl_->stop_fd) {
                    std::uint64_t value = 0U;
                    while (::read(impl_->stop_fd, &value, sizeof(value)) < 0 && errno == EINTR) {
                    }
                    impl_->stop_requested.store(true);
                    break;
                }
                if (fd == impl_->listen_fd) {
                    impl_->accept_clients();
                    continue;
                }
                if (impl_->clients.find(fd) == impl_->clients.end()) {
                    continue;
                }

                bool keep = true;
                if ((flags & EPOLLIN) != 0U) {
                    keep = impl_->read_client(fd);
                }
                if (keep && impl_->clients.find(fd) != impl_->clients.end() &&
                    (flags & EPOLLOUT) != 0U) {
                    keep = impl_->flush_client(fd);
                }
                if (keep && (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) {
                    keep = false;
                }
                if (!keep) {
                    impl_->disconnect_client(fd, true);
                }
            }
        }
    } catch (...) {
        impl_->cleanup_clients();
        impl_->is_running.store(false);
        throw;
    }

    impl_->cleanup_clients();
    impl_->is_running.store(false);
}

void ChatServer::request_stop() noexcept {
    impl_->stop_requested.store(true);
    if (impl_->stop_fd < 0) {
        return;
    }
    const std::uint64_t value = 1U;
    while (::write(impl_->stop_fd, &value, sizeof(value)) < 0 && errno == EINTR) {
    }
}

}  // namespace cppapplab::net
