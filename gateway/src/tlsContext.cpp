#include "tlsContext.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>

namespace gateway {

// OpenSSL initialization (call once at program startup)
struct OpenSSLInitializer {
    OpenSSLInitializer() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        spdlog::debug("OpenSSL initialized");
    }
    
    ~OpenSSLInitializer() {
        EVP_cleanup();
        ERR_free_strings();
        spdlog::debug("OpenSSL cleaned up");
    }
};

static OpenSSLInitializer g_openssl_init;

TLSContext::TLSContext(const std::string& cert_file, 
                       const std::string& key_file,
                       bool client_mtls)
    : client_mtls_(client_mtls) {
    
    spdlog::info("Initializing TLS context with cert: {}, key: {}, mTLS: {}",
                 cert_file, key_file, client_mtls);

    // Create SSL context with TLS server method
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) {
        throw std::runtime_error("Failed to create SSL_CTX: " + get_last_error());
    }

    // Configure TLS versions and cipher suites
    configure_tls_versions();
    configure_cipher_suites();

    // Load server certificate
    if (SSL_CTX_use_certificate_file(ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error("Failed to load certificate from " + cert_file + ": " + get_last_error());
    }

    // Load server private key
    if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error("Failed to load private key from " + key_file + ": " + get_last_error());
    }

    // Verify that private key matches certificate
    if (!SSL_CTX_check_private_key(ctx_)) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error("Private key does not match certificate: " + get_last_error());
    }

    // Configure client certificate verification (mTLS)
    if (client_mtls_) {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
        // TODO: Load CA certificates for client verification
        // SSL_CTX_load_verify_locations(ctx_, ca_file.c_str(), nullptr);
        spdlog::info("Client mTLS enabled - clients must present valid certificates");
    } else {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }

    // Enable session caching for performance
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);

    spdlog::info("TLS context initialized successfully");
}

TLSContext::~TLSContext() {
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

TLSContext::TLSContext(TLSContext&& other) noexcept
    : ctx_(other.ctx_), client_mtls_(other.client_mtls_) {
    other.ctx_ = nullptr;
}

TLSContext& TLSContext::operator=(TLSContext&& other) noexcept {
    if (this != &other) {
        if (ctx_) {
            SSL_CTX_free(ctx_);
        }
        ctx_ = other.ctx_;
        client_mtls_ = other.client_mtls_;
        other.ctx_ = nullptr;
    }
    return *this;
}

void TLSContext::configure_tls_versions() {
    // Enforce TLS 1.2 as minimum (TLS 1.0/1.1 deprecated)
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    
    // TODO: For production, enforce TLS 1.3 only
    // SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    
    spdlog::debug("TLS version range: TLS 1.2+");
}

void TLSContext::configure_cipher_suites() {
    // Modern cipher suite list (prioritize forward secrecy, AEAD)
    // Excludes weak ciphers (RC4, DES, MD5, NULL)
    const char* cipher_list = 
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305";
    
    if (SSL_CTX_set_cipher_list(ctx_, cipher_list) != 1) {
        spdlog::warn("Failed to set cipher list: {}", get_last_error());
    }

    // TLS 1.3 cipher suites (separate API)
    // const char* tls13_ciphers = "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256";
    // SSL_CTX_set_ciphersuites(ctx_, tls13_ciphers);

    spdlog::debug("Cipher suites configured (forward secrecy + AEAD)");
}

SSL* TLSContext::create_ssl(int socket_fd) {
    if (!ctx_) {
        throw std::runtime_error("TLS context not initialized");
    }

    SSL* ssl = SSL_new(ctx_);
    if (!ssl) {
        throw std::runtime_error("Failed to create SSL connection: " + get_last_error());
    }

    // Associate socket with SSL connection
    if (SSL_set_fd(ssl, socket_fd) != 1) {
        SSL_free(ssl);
        throw std::runtime_error("Failed to set SSL socket fd: " + get_last_error());
    }

    return ssl;
}

bool TLSContext::accept_handshake(SSL* ssl) {
    if (!ssl) {
        spdlog::error("Cannot perform handshake on null SSL connection");
        return false;
    }

    int result = SSL_accept(ssl);
    if (result <= 0) {
        int err = SSL_get_error(ssl, result);
        spdlog::error("TLS handshake failed: SSL_accept returned {}, error code: {}, msg: {}",
                     result, err, get_last_error());
        return false;
    }

    // Log handshake details
    const char* version = SSL_get_version(ssl);
    const char* cipher = SSL_get_cipher(ssl);
    spdlog::debug("TLS handshake successful: {} with {}", version, cipher);

    return true;
}

std::string TLSContext::get_last_error() {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "No error";
    }
    
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

// SSLConnection implementation

int SSLConnection::read(void* buffer, int size) {
    if (!ssl_) return -1;
    
    int n = SSL_read(ssl_, buffer, size);
    if (n <= 0) {
        int err = SSL_get_error(ssl_, n);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            spdlog::debug("SSL_read error: {}", err);
        }
        return -1;
    }
    return n;
}

int SSLConnection::write(const void* buffer, int size) {
    if (!ssl_) return -1;
    
    int n = SSL_write(ssl_, buffer, size);
    if (n <= 0) {
        int err = SSL_get_error(ssl_, n);
        if (err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ) {
            spdlog::debug("SSL_write error: {}", err);
        }
        return -1;
    }
    return n;
}

}  // namespace gateway
