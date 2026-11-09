#include "tcp_chat.hpp"

#include <stdexcept>

namespace cppapplab::net {

struct ChatServer::Impl {
    explicit Impl(ChatServerConfig server_config) : config(std::move(server_config)) {}

    ChatServerConfig config;
    std::uint16_t bound_port{0};
    bool is_running{false};
    bool stop_requested{false};
};

ChatServer::ChatServer(ChatServerConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {
    // TODO(W7-CHAT-1): validate resource limits and create non-blocking listener,
    // epoll instance, and eventfd stop channel. Register listener and stop channel.
}

ChatServer::~ChatServer() {
    request_stop();
    // TODO(W7-CHAT-2): release every descriptor after the event loop has exited.
}

std::uint16_t ChatServer::port() const noexcept {
    return impl_->bound_port;
}

bool ChatServer::running() const noexcept {
    return impl_->is_running;
}

void ChatServer::run() {
    // TODO(W7-CHAT-3): implement the epoll event loop and one-path cleanup.
    throw std::logic_error("TODO: implement ChatServer::run");
}

void ChatServer::request_stop() noexcept {
    // TODO(W7-CHAT-4): write to eventfd so epoll_wait wakes promptly.
    impl_->stop_requested = true;
}

}  // namespace cppapplab::net
