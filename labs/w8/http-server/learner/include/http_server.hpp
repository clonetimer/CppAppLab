#pragma once

#include "api_handlers.hpp"
#include "dispatcher.hpp"
#include "router.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace cppapplab::http {

using HttpLogSink = std::function<void(std::string_view)>;

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
    bool access_log_enabled{true};
    bool error_log_enabled{true};
    // Invoked on the Reactor thread. Keep the sink non-blocking; it must copy the view if retained.
    HttpLogSink log_sink{};
};

struct HttpServerMetrics {
    std::uint64_t accepted_connections{0U};
    std::uint64_t rejected_connections{0U};
    std::uint64_t active_connections{0U};
    std::uint64_t peak_active_connections{0U};
    std::uint64_t closed_connections{0U};
    std::uint64_t persistent_connections{0U};
    std::uint64_t requests_received{0U};
    std::uint64_t responses_completed{0U};
    std::uint64_t response_bytes{0U};
    std::uint64_t client_errors{0U};
    std::uint64_t server_errors{0U};
    std::uint64_t request_timeouts{0U};
    std::uint64_t idle_timeouts{0U};
    std::uint64_t handler_failures{0U};
    std::uint64_t queue_rejections{0U};
    std::uint64_t active_jobs{0U};
    std::uint64_t graceful_shutdowns{0U};
    std::uint64_t forced_shutdown_connections{0U};
    std::uint64_t log_sink_failures{0U};
    std::uint64_t write_backpressure_events{0U};
};

class HttpServer {
   public:
    explicit HttpServer(HttpServerConfig config = {}, Router router = make_default_router());
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    ~HttpServer();

    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] HttpServerMetrics metrics() const noexcept;
    void run();
    void request_stop() noexcept;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cppapplab::http
