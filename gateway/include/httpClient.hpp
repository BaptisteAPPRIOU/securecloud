#pragma once

#include <string>
#include <memory>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace gateway {

// HTTP request/response types for Boost.Beast (internal use only)
using BeastRequest = http::request<http::string_body>;
using BeastResponse = http::response<http::string_body>;

// HTTP client result with error handling
struct HttpResult {
    bool success{false};
    int status_code{0};
    std::string body;
    std::string error_message;
    std::unordered_map<std::string, std::string> headers;
};

/**
 * HTTP Client using Boost.Beast for upstream service communication
 * 
 * Features:
 * - Connection pooling for performance
 * - Configurable timeout for reliability
 * - Async I/O with io_context for non-blocking operations
 * - Support for GET/POST/PUT/DELETE/PATCH methods
 * - JSON request/response handling
 * - Custom headers support
 * - Keep-alive connections
 */
class HttpClient {
public:
    /**
     * Constructor
     * @param io_ctx Boost.Asio io_context for async operations
     * @param host Target host (e.g., "localhost", "10.0.1.5")
     * @param port Target port (e.g., 8080)
     * @param timeout_ms Request timeout in milliseconds (default: 5000ms)
     */
    HttpClient(net::io_context& io_ctx, 
               const std::string& host, 
               uint16_t port,
               std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000));

    ~HttpClient();

    // HTTP Methods
    HttpResult get(const std::string& path, 
                   const std::unordered_map<std::string, std::string>& headers = {});
    
    HttpResult post(const std::string& path, 
                    const nlohmann::json& body,
                    const std::unordered_map<std::string, std::string>& headers = {});
    
    HttpResult post(const std::string& path, 
                    const std::string& body,
                    const std::unordered_map<std::string, std::string>& headers = {});
    
    HttpResult put(const std::string& path, 
                   const nlohmann::json& body,
                   const std::unordered_map<std::string, std::string>& headers = {});
    
    HttpResult del(const std::string& path,
                   const std::unordered_map<std::string, std::string>& headers = {});
    
    HttpResult patch(const std::string& path,
                     const nlohmann::json& body,
                     const std::unordered_map<std::string, std::string>& headers = {});

    // Connection management
    void close();
    bool is_connected() const;

private:
    // Internal request execution
    HttpResult execute_request(http::verb method,
                               const std::string& path,
                               const std::string& body = "",
                               const std::unordered_map<std::string, std::string>& custom_headers = {});

    // Connection pool management
    void connect();
    void ensure_connected();

    // Member variables
    net::io_context& io_context_;
    std::string host_;
    uint16_t port_;
    std::chrono::milliseconds timeout_;
    
    // Connection state
    std::unique_ptr<tcp::socket> socket_;
    bool connected_{false};
};

/**
 * HTTP Connection Pool for managing multiple persistent connections
 * 
 * Features:
 * - Reuse connections for same host:port
 * - Automatic connection cleanup on errors
 * - Thread-safe access with mutex
 * - Configurable max connections per host
 */
class HttpConnectionPool {
public:
    HttpConnectionPool(net::io_context& io_ctx, size_t max_connections_per_host = 10);
    
    /**
     * Get or create HTTP client for target host
     * @param host Target hostname
     * @param port Target port
     * @return Shared pointer to HttpClient
     */
    std::shared_ptr<HttpClient> get_client(const std::string& host, uint16_t port);
    
    /**
     * Remove client from pool (e.g., after connection error)
     */
    void remove_client(const std::string& host, uint16_t port);
    
    /**
     * Clear all connections in pool
     */
    void clear();

private:
    std::string make_key(const std::string& host, uint16_t port) const;
    
    net::io_context& io_context_;
    size_t max_connections_per_host_;
    std::unordered_map<std::string, std::shared_ptr<HttpClient>> pool_;
    std::mutex mutex_;
};

} // namespace gateway
