#pragma once

#include "api_handlers.hpp"
#include "dispatcher.hpp"
#include "router.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace cppapplab::http {

struct HttpServerConfig {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{0U};
    int backlog{128};
    std::filesystem::path document_root{"www"};
    std::size_t worker_count{4U};
    std::size_t task_queue_capacity{64U};
    std::size_t max_connections{64U};
    std::size_t max_request_bytes{16U * 1024U + 1024U * 1024U};
    std::size_t max_response_bytes{2U * 1024U * 1024U};
    std::chrono::milliseconds idle_timeout{30000};
    std::chrono::milliseconds header_timeout{10000};
    std::chrono::milliseconds body_timeout{15000};
    std::size_t max_requests_per_connection{100U};
    std::size_t max_persistent_connections{32U};
    std::chrono::milliseconds shutdown_timeout{5000};
};

// TODO(student): declare HttpLogSink, HttpServerMetrics, log switches and metrics() API.
class HttpServer {
   public:
    explicit HttpServer(HttpServerConfig config = {}, Router router = make_default_router());
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    ~HttpServer();

    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    void run();
    void request_stop() noexcept;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cppapplab::http
