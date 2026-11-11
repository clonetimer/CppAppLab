#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cppapplab::http {

struct HttpLoadProbeConfig {
    std::string host{"127.0.0.1"};
    std::uint16_t port{0U};
    std::string target{"/health"};
    std::size_t client_count{1U};
    std::size_t requests_per_client{1U};
    std::chrono::milliseconds timeout{2000};
    int expected_status_code{200};
};

struct HttpLoadProbeResult {
    std::uint64_t requests_attempted{0U};
    std::uint64_t requests_succeeded{0U};
    std::uint64_t requests_failed{0U};
    std::uint64_t elapsed_microseconds{0U};
    double requests_per_second{0.0};
    std::uint64_t p50_latency_microseconds{0U};
    std::uint64_t p95_latency_microseconds{0U};
    std::uint64_t p99_latency_microseconds{0U};
};

[[nodiscard]] HttpLoadProbeResult run_http_load_probe(const HttpLoadProbeConfig& config);

}  // namespace cppapplab::http
