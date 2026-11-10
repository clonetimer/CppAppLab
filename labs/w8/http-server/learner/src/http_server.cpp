#include "http_server.hpp"

#include <stdexcept>

namespace cppapplab::http {

// TODO(D5-1): validate limits and create the non-blocking listener.
// TODO(D5-2): implement the single-owner epoll Reactor and generation tokens.
// TODO(D5-3): incrementally receive and parse one request per TCP connection.
// TODO(D5-4): submit owned requests to the W6 bounded ThreadPool.
// TODO(D5-5): return worker responses through a completion queue and eventfd.
// TODO(D5-6): implement bounded response buffers and partial sends.
// TODO(D5-7): map malformed/oversized/overloaded/exception paths to 400/413/503/500.
// TODO(D5-8): implement eventfd-driven stop and deterministic resource cleanup.

HttpServer::HttpServer(HttpServerConfig, Router) {
    throw std::logic_error("TODO: implement the W8-D5 HTTP TCP server");
}
HttpServer::~HttpServer() = default;
std::uint16_t HttpServer::port() const noexcept { return 0U; }
bool HttpServer::running() const noexcept { return false; }
void HttpServer::run() { throw std::logic_error("TODO: implement HttpServer::run"); }
void HttpServer::request_stop() noexcept {}

}  // namespace cppapplab::http
