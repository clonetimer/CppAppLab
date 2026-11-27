#include "tls_context.hpp"

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <utility>

namespace cppapplab::tls {
namespace {

bool valid_text(const std::string& value) {
    return !value.empty() && value.find('\0') == std::string::npos;
}

ContextResult context_error(TlsError error) {
    return {Context{}, error};
}

SessionResult session_error(TlsError error) {
    return {Session{}, error};
}

bool configure_protocol(SSL_CTX* context) {
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1) {
        return false;
    }
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
    return true;
}

}  // namespace

ContextResult make_server_context(const std::string& certificate,
                                  const std::string& private_key) {
    if (!valid_text(certificate) || !valid_text(private_key)) {
        return context_error(TlsError::InvalidArgument);
    }
    ERR_clear_error();
    Context context{SSL_CTX_new(TLS_server_method())};
    if (!context) {
        return context_error(TlsError::ContextCreation);
    }
    if (!configure_protocol(context.get())) {
        return context_error(TlsError::ProtocolConfiguration);
    }
    if (SSL_CTX_use_certificate_chain_file(context.get(), certificate.c_str()) != 1 ||
        SSL_CTX_use_PrivateKey_file(context.get(), private_key.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context.get()) != 1) {
        return context_error(TlsError::IdentityLoad);
    }
    return {std::move(context), TlsError::None};
}

ContextResult make_client_context(const std::string& trust_file) {
    if (!valid_text(trust_file)) {
        return context_error(TlsError::InvalidArgument);
    }
    ERR_clear_error();
    Context context{SSL_CTX_new(TLS_client_method())};
    if (!context) {
        return context_error(TlsError::ContextCreation);
    }
    if (!configure_protocol(context.get())) {
        return context_error(TlsError::ProtocolConfiguration);
    }
    if (SSL_CTX_load_verify_locations(context.get(), trust_file.c_str(), nullptr) != 1) {
        return context_error(TlsError::TrustLoad);
    }
    SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER, nullptr);
    return {std::move(context), TlsError::None};
}

SessionResult make_client_session(SSL_CTX* context, const std::string& peer_name) {
    if (context == nullptr || !valid_text(peer_name) || peer_name.size() > 253U ||
        (SSL_CTX_get_verify_mode(context) & SSL_VERIFY_PEER) == 0) {
        return session_error(TlsError::InvalidArgument);
    }
    ERR_clear_error();
    Session session{SSL_new(context)};
    if (!session) {
        return session_error(TlsError::SessionCreation);
    }
    if (SSL_is_server(session.get()) != 0) {
        return session_error(TlsError::InvalidArgument);
    }
    SSL_set_connect_state(session.get());
    SSL_set_hostflags(session.get(), X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (SSL_set1_host(session.get(), peer_name.c_str()) != 1 ||
        SSL_set_tlsext_host_name(session.get(), peer_name.c_str()) != 1) {
        return session_error(TlsError::PeerNameConfiguration);
    }
    return {std::move(session), TlsError::None};
}

}  // namespace cppapplab::tls
