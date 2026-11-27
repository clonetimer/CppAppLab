#include "tls_context.hpp"

namespace cppapplab::tls {

ContextResult make_server_context(const std::string& certificate,
                                  const std::string& private_key) {
    // TODO(W10-A1): allocate an owned server context, set the minimum version,
    // load the certificate/key and verify their match.
    (void)certificate;
    (void)private_key;
    return {Context{}, TlsError::InvalidArgument};
}

ContextResult make_client_context(const std::string& trust_file) {
    // TODO(W10-A1): use the explicit trust file and require peer verification.
    (void)trust_file;
    return {Context{}, TlsError::InvalidArgument};
}

SessionResult make_client_session(SSL_CTX* context, const std::string& peer_name) {
    // TODO(W10-A1): create an owned session, configure DNS identity and SNI.
    (void)context;
    (void)peer_name;
    return {Session{}, TlsError::InvalidArgument};
}

}  // namespace cppapplab::tls
