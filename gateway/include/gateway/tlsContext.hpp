#pragma once

#include <string>
#include <memory>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace gateway {

/**
 * TLS Context Manager
 * Manages OpenSSL SSL_CTX for HTTPS termination at the gateway.
 * 
 * Features:
 * - Server certificate/key loading
 * - Optional client certificate verification (mTLS)
 * - Secure cipher suite configuration
 * - TLS 1.2+ enforcement
 * 
 * Thread-safety: SSL_CTX is thread-safe for read operations after initialization.
 */
class TLSContext {
public:
    /**
     * Initialize TLS context with server certificates.
     * 
     * @param cert_file Path to PEM-encoded certificate file
     * @param key_file Path to PEM-encoded private key file
     * @param client_mtls Enable mutual TLS (client cert verification)
     * @throws std::runtime_error if initialization fails
     */
    TLSContext(const std::string& cert_file, 
               const std::string& key_file,
               bool client_mtls = false);
    
    ~TLSContext();

    // Non-copyable, movable
    TLSContext(const TLSContext&) = delete;
    TLSContext& operator=(const TLSContext&) = delete;
    TLSContext(TLSContext&&) noexcept;
    TLSContext& operator=(TLSContext&&) noexcept;

    /**
     * Create SSL connection for a socket file descriptor.
     * 
     * @param socket_fd Socket file descriptor
     * @return SSL* pointer (caller must SSL_free after use)
     * @throws std::runtime_error if SSL creation fails
     */
    SSL* create_ssl(int socket_fd);

    /**
     * Perform TLS handshake on SSL connection.
     * 
     * @param ssl SSL connection created by create_ssl()
     * @return true if handshake successful, false otherwise
     */
    bool accept_handshake(SSL* ssl);

    /**
     * Get last OpenSSL error as string.
     */
    static std::string get_last_error();

    /**
     * Check if TLS context is valid and ready.
     */
    bool is_valid() const { return ctx_ != nullptr; }

private:
    SSL_CTX* ctx_{nullptr};
    bool client_mtls_{false};

    void configure_cipher_suites();
    void configure_tls_versions();
};

/**
 * RAII wrapper for SSL* pointer.
 * Automatically calls SSL_free on destruction.
 */
class SSLConnection {
public:
    explicit SSLConnection(SSL* ssl) : ssl_(ssl) {}
    
    ~SSLConnection() {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
        }
    }

    // Non-copyable, movable
    SSLConnection(const SSLConnection&) = delete;
    SSLConnection& operator=(const SSLConnection&) = delete;
    SSLConnection(SSLConnection&& other) noexcept : ssl_(other.ssl_) {
        other.ssl_ = nullptr;
    }
    SSLConnection& operator=(SSLConnection&& other) noexcept {
        if (this != &other) {
            if (ssl_) {
                SSL_shutdown(ssl_);
                SSL_free(ssl_);
            }
            ssl_ = other.ssl_;
            other.ssl_ = nullptr;
        }
        return *this;
    }

    SSL* get() const { return ssl_; }
    SSL* release() {
        SSL* tmp = ssl_;
        ssl_ = nullptr;
        return tmp;
    }

    /**
     * Read data from SSL connection.
     * 
     * @param buffer Buffer to read into
     * @param size Buffer size
     * @return Number of bytes read, or -1 on error
     */
    int read(void* buffer, int size);

    /**
     * Write data to SSL connection.
     * 
     * @param buffer Data to write
     * @param size Data size
     * @return Number of bytes written, or -1 on error
     */
    int write(const void* buffer, int size);

private:
    SSL* ssl_{nullptr};
};

}  // namespace gateway
