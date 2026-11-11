#include "http_load_probe.hpp"

#include <stdexcept>

namespace cppapplab::http {

HttpLoadProbeResult run_http_load_probe(const HttpLoadProbeConfig& config) {
    (void)config;

    // TODO(student): validate the target, concurrency, request count and timeout.
    // TODO(student): run one persistent TCP client per worker using non-blocking I/O.
    // TODO(student): measure each response, classify status codes, and aggregate failures.
    // TODO(student): compute elapsed time, throughput, and p50/p95/p99 latency.
    throw std::logic_error("TODO(student): implement the HTTP load probe");
}

}  // namespace cppapplab::http
