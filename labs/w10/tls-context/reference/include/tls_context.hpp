#pragma once

#include <openssl/ssl.h>

#include <memory>
#include <string>

namespace cppapplab::tls {

struct ContextDeleter {
    void operator()(SSL_CTX* value) const noexcept { SSL_CTX_free(value); }
};
struct SessionDeleter {
    void operator()(SSL* value) const noexcept { SSL_free(value); }
};

using Context = std::unique_ptr<SSL_CTX, ContextDeleter>;
using Session = std::unique_ptr<SSL, SessionDeleter>;

enum class TlsError {
    None,
    InvalidArgument,
    ContextCreation,
    ProtocolConfiguration,
    IdentityLoad,
    TrustLoad,
    SessionCreation,
    PeerNameConfiguration,
};

struct ContextResult {
    Context context;
    TlsError error{TlsError::None};
};

struct SessionResult {
    Session session;
    TlsError error{TlsError::None};
};

// Local teaching contexts: server authenticates itself; client verifies the
// explicit trust file and DNS peer name. No client certificate authentication.
[[nodiscard]] ContextResult make_server_context(const std::string& certificate,
                                                const std::string& private_key);
[[nodiscard]] ContextResult make_client_context(const std::string& trust_file);
[[nodiscard]] SessionResult make_client_session(SSL_CTX* context,
                                                const std::string& peer_name);

}  // namespace cppapplab::tls
