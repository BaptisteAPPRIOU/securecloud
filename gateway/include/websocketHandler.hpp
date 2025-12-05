#pragma once

#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <functional>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace gateway {

/**
 * WebSocket connection for bidirectional client communication
 * 
 * Features:
 * - RFC 6455 compliant WebSocket frames
 * - Ping/pong for connection health
 * - Text and binary message support
 * - Automatic fragmentation handling
 * - Connection lifecycle management
 */
class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection> {
public:
    using MessageCallback = std::function<void(const std::string& message)>;
    using CloseCallback = std::function<void()>;
    
    WebSocketConnection(tcp::socket socket);
    
    /**
     * Perform WebSocket handshake (upgrade from HTTP)
     */
    void accept();
    
    /**
     * Send text message to client
     */
    void send(const std::string& message);
    
    /**
     * Send JSON message to client
     */
    void send(const nlohmann::json& json_message);
    
    /**
     * Start reading messages asynchronously
     */
    void start_read(MessageCallback on_message, CloseCallback on_close);
    
    /**
     * Close WebSocket connection gracefully
     */
    void close();
    
    /**
     * Get connection metadata
     */
    std::string get_remote_address() const;
    bool is_open() const;
    
    /**
     * Set user ID associated with this connection (for session management)
     */
    void set_user_id(const std::string& user_id) { user_id_ = user_id; }
    std::string get_user_id() const { return user_id_; }

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::string user_id_;
    
    MessageCallback message_callback_;
    CloseCallback close_callback_;
    
    bool reading_{false};
};

/**
 * WebSocket Session Manager
 * 
 * Manages active WebSocket connections and enables:
 * - User session tracking (user_id → connections)
 * - Event broadcasting to specific users or all users
 * - Connection lifecycle management
 * - Cleanup of disconnected sessions
 */
class WebSocketSessionManager {
public:
    /**
     * Register a new WebSocket connection
     * @param user_id User identifier from JWT
     * @param connection WebSocket connection instance
     */
    void add_session(const std::string& user_id, std::shared_ptr<WebSocketConnection> connection);
    
    /**
     * Remove session when connection closes
     */
    void remove_session(const std::string& user_id, std::shared_ptr<WebSocketConnection> connection);
    
    /**
     * Send message to specific user (all their connections)
     * @param user_id Target user
     * @param message Message to send
     * @return Number of connections that received the message
     */
    size_t send_to_user(const std::string& user_id, const nlohmann::json& message);
    
    /**
     * Broadcast message to all connected users
     * @param message Message to broadcast
     * @return Number of connections that received the message
     */
    size_t broadcast(const nlohmann::json& message);
    
    /**
     * Get number of active connections for a user
     */
    size_t get_user_connection_count(const std::string& user_id) const;
    
    /**
     * Get total number of active connections
     */
    size_t get_total_connection_count() const;
    
    /**
     * Get all active user IDs
     */
    std::vector<std::string> get_active_users() const;

private:
    // user_id → list of connections (users can have multiple devices/tabs)
    std::unordered_map<std::string, std::vector<std::shared_ptr<WebSocketConnection>>> sessions_;
    mutable std::mutex mutex_;
};

/**
 * WebSocket Handler for managing WebSocket upgrades and routing
 * 
 * Integrates with HttpServer to handle:
 * - HTTP → WebSocket upgrade (101 Switching Protocols)
 * - Route-based WebSocket endpoints
 * - Message routing to microservices
 * - Event streaming from microservices to clients
 */
class WebSocketHandler {
public:
    WebSocketHandler(net::io_context& io_ctx);
    
    /**
     * Handle WebSocket upgrade request
     * @param socket TCP socket from HTTP connection
     * @param user_id Authenticated user ID from JWT
     * @return true if upgrade successful
     */
    bool handle_upgrade(tcp::socket socket, const std::string& user_id);
    
    /**
     * Get session manager for broadcasting events
     */
    WebSocketSessionManager& get_session_manager() { return session_manager_; }

private:
    void on_message(const std::string& user_id, const std::string& message);
    void on_close(const std::string& user_id, std::shared_ptr<WebSocketConnection> connection);
    
    net::io_context& io_context_;
    WebSocketSessionManager session_manager_;
};

} // namespace gateway
