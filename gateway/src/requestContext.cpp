#include "requestContext.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

RequestContext::RequestContext() 
    : request_id_(generate_request_id()),
      correlation_id_(request_id_), // Default to request_id if not set
      timestamp_(std::chrono::system_clock::now()) {
}

RequestContext::RequestContext(const std::string& client_ip)
    : request_id_(generate_request_id()),
      correlation_id_(request_id_),
      client_ip_(client_ip),
      timestamp_(std::chrono::system_clock::now()) {
}

void RequestContext::set_correlation_id(const std::string& correlation_id) {
    if (!correlation_id.empty()) {
        correlation_id_ = correlation_id;
    }
}

std::string RequestContext::generate_request_id() {
    // Generate a simple UUID-like ID: timestamp-random
    static thread_local std::mt19937_64 generator(
        std::random_device{}() ^ 
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    uint64_t random_part = generator();
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(12) << ms
        << "-"
        << std::setw(16) << random_part;
    
    return oss.str();
}

std::string RequestContext::format() const {
    std::ostringstream oss;
    oss << "[req_id=" << request_id_;
    if (correlation_id_ != request_id_) {
        oss << " corr_id=" << correlation_id_;
    }
    if (!client_ip_.empty()) {
        oss << " client=" << client_ip_;
    }
    oss << "]";
    return oss.str();
}

} // namespace gateway
