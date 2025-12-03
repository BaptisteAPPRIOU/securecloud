#include "gateway/httpClient.hpp"
#include <spdlog/spdlog.h>
#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

namespace gateway {

// ============================================================================
// HttpClient Implementation
// ============================================================================

HttpClient::HttpClient(net::io_context& io_ctx, 
                       const std::string& host, 
                       uint16_t port,
                       std::chrono::milliseconds timeout_ms)
    : io_context_(io_ctx), host_(host), port_(port), timeout_(timeout_ms) {
    spdlog::debug("HttpClient created for {}:{}", host_, port_);
}

HttpClient::~HttpClient() {
    close();
}

void HttpClient::connect() {
    if (connected_) {
        return;
    }

    try {
        // Resolve hostname
        tcp::resolver resolver(io_context_);
        auto const results = resolver.resolve(host_, std::to_string(port_));
        
        // Create socket
        socket_ = std::make_unique<tcp::socket>(io_context_);
        
        // Connect with timeout
        net::connect(*socket_, results.begin(), results.end());
        
        connected_ = true;
        spdlog::info("Connected to {}:{}", host_, port_);
    } catch (const std::exception& e) {
        connected_ = false;
        spdlog::error("Connection failed to {}:{} - {}", host_, port_, e.what());
        throw;
    }
}

void HttpClient::ensure_connected() {
    if (!connected_ || !socket_ || !socket_->is_open()) {
        connect();
    }
}

void HttpClient::close() {
    if (socket_ && socket_->is_open()) {
        beast::error_code ec;
        socket_->shutdown(tcp::socket::shutdown_both, ec);
        socket_->close(ec);
        connected_ = false;
        spdlog::debug("Connection closed to {}:{}", host_, port_);
    }
}

bool HttpClient::is_connected() const {
    return connected_ && socket_ && socket_->is_open();
}

HttpResult HttpClient::execute_request(http::verb method,
                                       const std::string& path,
                                       const std::string& body,
                                       const std::unordered_map<std::string, std::string>& custom_headers) {
    HttpResult result;
    
    try {
        ensure_connected();
        
        // Build HTTP request
        BeastRequest req{method, path, 11}; // HTTP/1.1
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "SecureCloudGateway/1.0");
        req.set(http::field::connection, "keep-alive");
        
        // Add custom headers
        for (const auto& [key, value] : custom_headers) {
            req.set(key, value);
        }
        
        // Set body if provided
        if (!body.empty()) {
            req.body() = body;
            req.set(http::field::content_type, "application/json");
            req.prepare_payload();
        }
        
        spdlog::debug("Sending {} request to {}:{}{}", 
                      std::string(http::to_string(method)), host_, port_, path);
        
        // Send request
        http::write(*socket_, req);
        
        // Receive response
        beast::flat_buffer buffer;
        BeastResponse res;
        http::read(*socket_, buffer, res);
        
        // Parse response
        result.success = true;
        result.status_code = res.result_int();
        result.body = res.body();
        
        // Extract headers
        for (auto const& field : res) {
            result.headers[std::string(field.name_string())] = std::string(field.value());
        }
        
        spdlog::debug("Received response: status={}, body_size={}", 
                      result.status_code, result.body.size());
        
    } catch (const beast::system_error& e) {
        result.success = false;
        result.error_message = e.what();
        spdlog::error("HTTP request failed: {}", e.what());
        
        // Close connection on error
        close();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        spdlog::error("HTTP request exception: {}", e.what());
        close();
    }
    
    return result;
}

HttpResult HttpClient::get(const std::string& path, 
                           const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::get, path, "", headers);
}

HttpResult HttpClient::post(const std::string& path, 
                            const nlohmann::json& body,
                            const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::post, path, body.dump(), headers);
}

HttpResult HttpClient::post(const std::string& path, 
                            const std::string& body,
                            const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::post, path, body, headers);
}

HttpResult HttpClient::put(const std::string& path, 
                           const nlohmann::json& body,
                           const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::put, path, body.dump(), headers);
}

HttpResult HttpClient::del(const std::string& path,
                           const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::delete_, path, "", headers);
}

HttpResult HttpClient::patch(const std::string& path,
                             const nlohmann::json& body,
                             const std::unordered_map<std::string, std::string>& headers) {
    return execute_request(http::verb::patch, path, body.dump(), headers);
}

// ============================================================================
// HttpConnectionPool Implementation
// ============================================================================

HttpConnectionPool::HttpConnectionPool(net::io_context& io_ctx, size_t max_connections_per_host)
    : io_context_(io_ctx), max_connections_per_host_(max_connections_per_host) {
    spdlog::info("HttpConnectionPool initialized with max_connections={}", max_connections_per_host_);
}

std::string HttpConnectionPool::make_key(const std::string& host, uint16_t port) const {
    return host + ":" + std::to_string(port);
}

std::shared_ptr<HttpClient> HttpConnectionPool::get_client(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(host, port);
    
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        // Reuse existing connection
        spdlog::debug("Reusing connection from pool for {}", key);
        return it->second;
    }
    
    // Create new client
    auto client = std::make_shared<HttpClient>(io_context_, host, port);
    pool_[key] = client;
    
    spdlog::info("Created new connection in pool for {} (pool_size={})", key, pool_.size());
    
    return client;
}

void HttpConnectionPool::remove_client(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(host, port);
    auto it = pool_.find(key);
    
    if (it != pool_.end()) {
        it->second->close();
        pool_.erase(it);
        spdlog::info("Removed client from pool for {} (pool_size={})", key, pool_.size());
    }
}

void HttpConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [key, client] : pool_) {
        client->close();
    }
    
    pool_.clear();
    spdlog::info("Cleared all connections from pool");
}

} // namespace gateway
