#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace cppapplab::net {
struct ChatServerConfig {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{0};
    int backlog{64};
    std::size_t max_clients{64U};
    std::size_t max_line_bytes{4096U};
    std::size_t max_pending_bytes{65536U};
};
class ChatServer {
   public:
    explicit ChatServer(ChatServerConfig config = {});
    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;
    ~ChatServer();
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    void run();
    void request_stop() noexcept;
   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace cppapplab::net
