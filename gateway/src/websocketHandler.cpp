#include "gateway/websocketHandler.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

// ============================================================================
// WebSocketConnection Implementation
// ============================================================================

WebSocketConnection::WebSocketConnection(tcp::socket socket)
    : ws_(std::move(socket)) {
    spdlog::debug("WebSocketConnection created");
}

void WebSocketConnection::accept() {
    try {
        ws_.accept();
        spdlog::info("WebSocket handshake completed for {}", get_remote_address());
    } catch (const std::exception& e) {
        spdlog::error("WebSocket accept failed: {}", e.what());
        throw;
    }
}

void WebSocketConnection::send(const std::string& message) {
    try {
        ws_.write(net::buffer(message));
        spdlog::debug("Sent WebSocket message: {} bytes", message.size());
    } catch (const std::exception& e) {
        spdlog::error("WebSocket send failed: {}", e.what());
        close();
    }
}

void WebSocketConnection::send(const nlohmann::json& json_message) {
    send(json_message.dump());
}

void WebSocketConnection::start_read(MessageCallback on_message, CloseCallback on_close) {
    message_callback_ = std::move(on_message);
    close_callback_ = std::move(on_close);
    reading_ = true;
    do_read();
}

void WebSocketConnection::do_read() {
    auto self = shared_from_this();
    
    ws_.async_read(
        buffer_,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        }
    );
}

void WebSocketConnection::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            spdlog::info("WebSocket closed gracefully: {}", get_remote_address());
        } else {
            spdlog::error("WebSocket read error: {}", ec.message());
        }
        
        if (close_callback_) {
            close_callback_();
        }
        return;
    }
    
    // Extract message
    std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    
    spdlog::debug("Received WebSocket message: {} bytes from {}", bytes_transferred, get_remote_address());
    
    // Invoke callback
    if (message_callback_) {
        message_callback_(message);
    }
    
    // Continue reading
    if (reading_) {
        do_read();
    }
}

void WebSocketConnection::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        spdlog::error("WebSocket write error: {}", ec.message());
        close();
    }
}

void WebSocketConnection::close() {
    if (ws_.is_open()) {
        try {
            reading_ = false;
            ws_.close(websocket::close_code::normal);
            spdlog::info("WebSocket connection closed: {}", get_remote_address());
        } catch (const std::exception& e) {
            spdlog::error("WebSocket close error: {}", e.what());
        }
    }
}

std::string WebSocketConnection::get_remote_address() const {
    try {
        auto endpoint = ws_.next_layer().remote_endpoint();
        return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    } catch (...) {
        return "unknown";
    }
}

bool WebSocketConnection::is_open() const {
    return ws_.is_open();
}

// ============================================================================
// WebSocketSessionManager Implementation
// ============================================================================

void WebSocketSessionManager::add_session(const std::string& user_id, 
                                          std::shared_ptr<WebSocketConnection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sessions_[user_id].push_back(connection);
    
    spdlog::info("Added WebSocket session for user={} (total_sessions={}, user_connections={})",
                 user_id, get_total_connection_count(), sessions_[user_id].size());
}

void WebSocketSessionManager::remove_session(const std::string& user_id,
                                             std::shared_ptr<WebSocketConnection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(user_id);
    if (it != sessions_.end()) {
        auto& connections = it->second;
        connections.erase(
            std::remove(connections.begin(), connections.end(), connection),
            connections.end()
        );
        
        // Remove user entry if no more connections
        if (connections.empty()) {
            sessions_.erase(it);
        }
        
        spdlog::info("Removed WebSocket session for user={} (total_sessions={})",
                     user_id, get_total_connection_count());
    }
}

size_t WebSocketSessionManager::send_to_user(const std::string& user_id, 
                                              const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(user_id);
    if (it == sessions_.end()) {
        spdlog::debug("No active sessions for user={}", user_id);
        return 0;
    }
    
    size_t sent_count = 0;
    std::string message_str = message.dump();
    
    for (auto& connection : it->second) {
        if (connection && connection->is_open()) {
            connection->send(message_str);
            sent_count++;
        }
    }
    
    spdlog::debug("Sent message to user={} ({} connections)", user_id, sent_count);
    return sent_count;
}

size_t WebSocketSessionManager::broadcast(const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t sent_count = 0;
    std::string message_str = message.dump();
    
    for (auto& [user_id, connections] : sessions_) {
        for (auto& connection : connections) {
            if (connection && connection->is_open()) {
                connection->send(message_str);
                sent_count++;
            }
        }
    }
    
    spdlog::info("Broadcast message to {} connections", sent_count);
    return sent_count;
}

size_t WebSocketSessionManager::get_user_connection_count(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(user_id);
    if (it != sessions_.end()) {
        return it->second.size();
    }
    return 0;
}

size_t WebSocketSessionManager::get_total_connection_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total = 0;
    for (const auto& [user_id, connections] : sessions_) {
        total += connections.size();
    }
    return total;
}

std::vector<std::string> WebSocketSessionManager::get_active_users() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> users;
    users.reserve(sessions_.size());
    
    for (const auto& [user_id, _] : sessions_) {
        users.push_back(user_id);
    }
    
    return users;
}

// ============================================================================
// WebSocketHandler Implementation
// ============================================================================

WebSocketHandler::WebSocketHandler(net::io_context& io_ctx)
    : io_context_(io_ctx) {
    spdlog::info("WebSocketHandler initialized");
}

bool WebSocketHandler::handle_upgrade(tcp::socket socket, const std::string& user_id) {
    try {
        // Create WebSocket connection
        auto connection = std::make_shared<WebSocketConnection>(std::move(socket));
        connection->set_user_id(user_id);
        
        // Perform handshake
        connection->accept();
        
        // Register session
        session_manager_.add_session(user_id, connection);
        
        // Start reading messages
        connection->start_read(
            [this, user_id](const std::string& message) {
                this->on_message(user_id, message);
            },
            [this, user_id, connection]() {
                this->on_close(user_id, connection);
            }
        );
        
        spdlog::info("WebSocket upgrade successful for user={}", user_id);
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("WebSocket upgrade failed for user={}: {}", user_id, e.what());
        return false;
    }
}

void WebSocketHandler::on_message(const std::string& user_id, const std::string& message) {
    spdlog::debug("WebSocket message from user={}: {}", user_id, message);
    
    // TODO: Route message to appropriate microservice based on message type
    // For now, just log the message
    
    try {
        nlohmann::json msg = nlohmann::json::parse(message);
        
        // Example message routing based on "type" field
        if (msg.contains("type")) {
            std::string msg_type = msg["type"];
            
            if (msg_type == "ping") {
                // Echo pong back
                nlohmann::json pong = {{"type", "pong"}, {"timestamp", msg["timestamp"]}};
                session_manager_.send_to_user(user_id, pong);
            }
            // TODO: Add routing for other message types (chat, file upload, etc.)
        }
        
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("Invalid JSON message from user={}: {}", user_id, e.what());
    }
}

void WebSocketHandler::on_close(const std::string& user_id, 
                                std::shared_ptr<WebSocketConnection> connection) {
    spdlog::info("WebSocket connection closed for user={}", user_id);
    session_manager_.remove_session(user_id, connection);
}

} // namespace gateway
