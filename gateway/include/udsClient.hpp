#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using unix_socket = net::local::stream_protocol;

namespace gateway {

/**
 * Unix Domain Socket (UDS) Client for high-performance local IPC
 * 
 * Features:
 * - AF_UNIX socket communication for low-latency microservice calls
 * - Request/response pattern with length-prefixed messages
 * - Configurable timeout for reliability
 * - JSON serialization/deserialization
 * - Connection pooling support
 * 
 * Message Protocol:
 * [4 bytes: length][N bytes: JSON payload]
 * - Length is network byte order (big-endian) uint32_t
 * - Payload is UTF-8 encoded JSON
 */
class UdsClient {
public:
    /**
     * Constructor
     * @param io_ctx Boost.Asio io_context
     * @param socket_path Path to Unix socket (e.g., "/tmp/auth-service.sock")
     * @param timeout_ms Request timeout in milliseconds
     */
    UdsClient(net::io_context& io_ctx,
              const std::string& socket_path,
              std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(3000));
    
    ~UdsClient();

    /**
     * Send JSON request and receive JSON response
     * @param request JSON request object
     * @return JSON response or nullopt on error
     */
    std::optional<nlohmann::json> send_request(const nlohmann::json& request);
    
    /**
     * Send raw string request and receive raw string response
     * @param request Raw request data
     * @return Response string or nullopt on error
     */
    std::optional<std::string> send_raw(const std::string& request);
    
    /**
     * Check if socket is connected
     */
    bool is_connected() const;
    
    /**
     * Close the socket connection
     */
    void close();

private:
    void connect();
    void ensure_connected();
    
    // Protocol helpers
    bool send_message(const std::string& message);
    std::optional<std::string> receive_message();
    
    net::io_context& io_context_;
    std::string socket_path_;
    std::chrono::milliseconds timeout_;
    
    std::unique_ptr<unix_socket::socket> socket_;
    bool connected_{false};
};

} // namespace gateway
