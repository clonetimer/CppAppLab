#include "tls_context.hpp"

#include <gtest/gtest.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using namespace cppapplab::tls;

std::string cert(const char* filename) {
    return std::string(CPPAPPLAB_TLS_CERT_DIR) + "/" + filename;
}

ContextResult server_context() {
    return make_server_context(cert("server.pem"), cert("server.key"));
}

enum class Handshake { Complete, Failed, BudgetExhausted };

// A fixed in-memory full-duplex transport exercises real TLS records without
// sockets, sleeps, remote hosts, or application-controlled test certificates.
bool attach_pair(SSL* client, SSL* server) {
    BIO* client_bio = nullptr;
    BIO* server_bio = nullptr;
    if (BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) != 1) {
        BIO_free(client_bio);
        BIO_free(server_bio);
        return false;
    }
    SSL_set_bio(client, client_bio, client_bio);
    SSL_set_bio(server, server_bio, server_bio);
    SSL_set_accept_state(server);
    return true;
}

Handshake handshake(SSL* client, SSL* server) {
    for (unsigned step = 0; step < 128U; ++step) {
        for (SSL* endpoint : {client, server}) {
            if (SSL_is_init_finished(endpoint) != 0) {
                continue;
            }
            ERR_clear_error();
            const int result = SSL_do_handshake(endpoint);
            if (result == 1) {
                continue;
            }
            const int error = SSL_get_error(endpoint, result);
            if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
                return Handshake::Failed;
            }
        }
        if (SSL_is_init_finished(client) != 0 && SSL_is_init_finished(server) != 0) {
            return Handshake::Complete;
        }
    }
    return Handshake::BudgetExhausted;
}

struct Pair {
    Context server_context;
    Context client_context;
    Session server;
    Session client;
};

Pair make_pair(const char* trust_name, const std::string& hostname,
               int max_version = 0) {
    auto server = server_context();
    auto client = make_client_context(cert(trust_name));
    if (!server.context || !client.context) {
        return {};
    }
    if (max_version != 0 &&
        SSL_CTX_set_max_proto_version(client.context.get(), max_version) != 1) {
        return {};
    }
    auto connection = make_client_session(client.context.get(), hostname);
    Session server_session{SSL_new(server.context.get())};
    if (!server_session || !connection.session ||
        !attach_pair(connection.session.get(), server_session.get())) {
        return {};
    }
    return {std::move(server.context), std::move(client.context),
            std::move(server_session), std::move(connection.session)};
}

TEST(TlsContext, OwnedTypesAreMoveOnly) {
    static_assert(!std::is_copy_constructible_v<Context>);
    static_assert(!std::is_copy_constructible_v<Session>);
    static_assert(std::is_move_constructible_v<Context>);
    static_assert(std::is_move_constructible_v<Session>);
    auto result = server_context();
    ASSERT_TRUE(result.context);
    Context moved = std::move(result.context);
    EXPECT_FALSE(result.context);
    EXPECT_TRUE(moved);
}

TEST(TlsContext, ServerIdentityAndProtocol) {
    auto result = server_context();
    ASSERT_TRUE(result.context);
    EXPECT_EQ(result.error, TlsError::None);
    EXPECT_EQ(SSL_CTX_get_min_proto_version(result.context.get()), TLS1_2_VERSION);
    EXPECT_NE(SSL_CTX_get_options(result.context.get()) & SSL_OP_NO_COMPRESSION, 0U);
    EXPECT_EQ(SSL_CTX_check_private_key(result.context.get()), 1);
}

TEST(TlsContext, EmptyOrEmbeddedNullIdentity) {
    EXPECT_EQ(make_server_context("", cert("server.key")).error, TlsError::InvalidArgument);
    EXPECT_EQ(make_server_context(cert("server.pem"), "").error, TlsError::InvalidArgument);
    const auto value = cert("server.pem") + std::string("\0suffix", 7);
    const auto result = make_server_context(value, cert("server.key"));
    EXPECT_EQ(result.error, TlsError::InvalidArgument);
    EXPECT_FALSE(result.context);
}

TEST(TlsContext, MissingCertificate) {
    const auto result = make_server_context(cert("missing.pem"), cert("server.key"));
    EXPECT_EQ(result.error, TlsError::IdentityLoad);
    EXPECT_FALSE(result.context);
}

TEST(TlsContext, MissingPrivateKey) {
    const auto result = make_server_context(cert("server.pem"), cert("missing.key"));
    EXPECT_EQ(result.error, TlsError::IdentityLoad);
    EXPECT_FALSE(result.context);
}

TEST(TlsContext, MismatchedKey) {
    const auto result = make_server_context(cert("server.pem"), cert("other.key"));
    EXPECT_EQ(result.error, TlsError::IdentityLoad);
    EXPECT_FALSE(result.context);
}

TEST(TlsContext, ClientVerificationAndProtocol) {
    auto result = make_client_context(cert("ca.pem"));
    ASSERT_TRUE(result.context);
    EXPECT_EQ(result.error, TlsError::None);
    EXPECT_NE(SSL_CTX_get_verify_mode(result.context.get()) & SSL_VERIFY_PEER, 0);
    EXPECT_EQ(SSL_CTX_get_min_proto_version(result.context.get()), TLS1_2_VERSION);
    EXPECT_NE(SSL_CTX_get_options(result.context.get()) & SSL_OP_NO_COMPRESSION, 0U);
}

TEST(TlsContext, MissingTrustFailsClosed) {
    const auto missing = make_client_context(cert("missing.pem"));
    EXPECT_EQ(missing.error, TlsError::TrustLoad);
    EXPECT_FALSE(missing.context);
    EXPECT_EQ(make_client_context("").error, TlsError::InvalidArgument);
}

TEST(TlsContext, InvalidPeerNameAndContext) {
    auto result = make_client_context(cert("ca.pem"));
    ASSERT_TRUE(result.context);
    for (const auto& name : {std::string{}, std::string("localhost\0suffix", 16),
                             std::string(254U, 'a')}) {
        const auto session = make_client_session(result.context.get(), name);
        EXPECT_EQ(session.error, TlsError::InvalidArgument);
        EXPECT_FALSE(session.session);
    }
    EXPECT_EQ(make_client_session(nullptr, "localhost").error, TlsError::InvalidArgument);
}

TEST(TlsContext, UnverifiedContextRejected) {
    Context raw{SSL_CTX_new(TLS_client_method())};
    ASSERT_TRUE(raw);
    ASSERT_EQ(SSL_CTX_get_verify_mode(raw.get()), SSL_VERIFY_NONE);
    const auto result = make_client_session(raw.get(), "localhost");
    EXPECT_EQ(result.error, TlsError::InvalidArgument);
    EXPECT_FALSE(result.session);
}

TEST(TlsContext, ServerContextRejectedAsClient) {
    auto result = server_context();
    ASSERT_TRUE(result.context);
    SSL_CTX_set_verify(result.context.get(), SSL_VERIFY_PEER, nullptr);
    const auto session = make_client_session(result.context.get(), "localhost");
    EXPECT_EQ(session.error, TlsError::InvalidArgument);
    EXPECT_FALSE(session.session);
}

TEST(TlsHandshake, TrustedPeerAndSni) {
    auto pair = make_pair("ca.pem", "localhost");
    ASSERT_TRUE(pair.client);
    ASSERT_TRUE(pair.server);
    ASSERT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Complete);
    EXPECT_EQ(SSL_get_verify_result(pair.client.get()), X509_V_OK);
    EXPECT_GE(SSL_version(pair.client.get()), TLS1_2_VERSION);
    EXPECT_STREQ(SSL_get_servername(pair.server.get(), TLSEXT_NAMETYPE_host_name), "localhost");
}

TEST(TlsHandshake, WrongDnsIdentityRejected) {
    auto pair = make_pair("ca.pem", "other.test");
    ASSERT_TRUE(pair.client);
    ASSERT_TRUE(pair.server);
    EXPECT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Failed);
    EXPECT_EQ(SSL_get_verify_result(pair.client.get()), X509_V_ERR_HOSTNAME_MISMATCH);
}

TEST(TlsHandshake, UnrelatedTrustAnchorRejected) {
    auto pair = make_pair("other.pem", "localhost");
    ASSERT_TRUE(pair.client);
    ASSERT_TRUE(pair.server);
    EXPECT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Failed);
    EXPECT_NE(SSL_get_verify_result(pair.client.get()), X509_V_OK);
}

TEST(TlsHandshake, ExplicitTls12Negotiation) {
    auto pair = make_pair("ca.pem", "localhost", TLS1_2_VERSION);
    ASSERT_TRUE(pair.client);
    ASSERT_TRUE(pair.server);
    ASSERT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Complete);
    EXPECT_EQ(SSL_version(pair.client.get()), TLS1_2_VERSION);
}

TEST(TlsHandshake, RealBinaryApplicationData) {
    auto pair = make_pair("ca.pem", "localhost", TLS1_2_VERSION);
    ASSERT_TRUE(pair.client);
    ASSERT_TRUE(pair.server);
    ASSERT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Complete);
    const std::array<unsigned char, 4> input{42, 0, 255, 7};
    std::array<unsigned char, 4> output{};
    std::size_t written = 0;
    ASSERT_EQ(SSL_write_ex(pair.client.get(), input.data(), input.size(), &written), 1);
    ASSERT_EQ(written, input.size());
    std::size_t received = 0;
    ASSERT_EQ(SSL_read_ex(pair.server.get(), output.data(), output.size(), &received), 1);
    EXPECT_EQ(received, input.size());
    EXPECT_EQ(output, input);
}

TEST(TlsHandshake, RepeatedOwnershipAndCleanup) {
    for (unsigned attempt = 0; attempt < 4U; ++attempt) {
        auto pair = make_pair("ca.pem", "localhost");
        ASSERT_TRUE(pair.client);
        ASSERT_TRUE(pair.server);
        EXPECT_EQ(handshake(pair.client.get(), pair.server.get()), Handshake::Complete);
    }
}
}  // namespace
