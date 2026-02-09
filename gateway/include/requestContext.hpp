#pragma once
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace gateway {

/**
 * RequestContext holds per-request context information including:
 * - Unique request ID (UUID-like format)
 * - Correlation ID (from X-Correlation-ID header or generated)
 * - Request timestamp
 * - Client IP address
 */
class RequestContext {
public:
    RequestContext();
    explicit RequestContext(const std::string& client_ip);
    
    // Set correlation ID from incoming request header
    void set_correlation_id(const std::string& correlation_id);
    
    // Getters
    const std::string& request_id() const { return request_id_; }
    const std::string& correlation_id() const { return correlation_id_; }
    const std::string& client_ip() const { return client_ip_; }
    std::chrono::system_clock::time_point timestamp() const { return timestamp_; }
    
    // Format for logging
    std::string format() const;
    
private:
    std::string generate_request_id();
    
    std::string request_id_;
    std::string correlation_id_;
    std::string client_ip_;
    std::chrono::system_clock::time_point timestamp_;
};

} // namespace gateway
