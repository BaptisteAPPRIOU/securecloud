#include "rateLimitMiddleware.hpp"
#include "gatewayMetrics.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace gateway {

RateLimitMiddleware::RateLimitMiddleware(MultiStrategyRateLimiter& limiter)
    : limiter_(limiter) {
    spdlog::info("RateLimitMiddleware initialized");
}

RateLimitResult RateLimitMiddleware::check_rate_limit(const Request& request,
                                                      const std::string& client_ip,
                                                      const std::string& user_id,
                                                      const std::string& endpoint) {
    auto result = limiter_.check(client_ip, user_id, endpoint);
    
    if (!result.allowed) {
        spdlog::warn("Rate limit triggered - ip={}, user={}, endpoint={}, reason={}",
                     client_ip, user_id, endpoint, result.reason);
        
        // Record rate limit metric by strategy
        GatewayMetrics::instance().record_rate_limit(result.reason);
    }
    
    return result;
}

std::string RateLimitMiddleware::extract_client_ip(const Request& request, 
                                                   const std::string& socket_ip) {
    // Check X-Forwarded-For header (proxy/load balancer)
    auto xff_it = request.headers.find("x-forwarded-for");
    if (xff_it != request.headers.end()) {
        // X-Forwarded-For can be comma-separated list, take first IP
        std::string xff = xff_it->second;
        auto comma_pos = xff.find(',');
        if (comma_pos != std::string::npos) {
            return xff.substr(0, comma_pos);
        }
        return xff;
    }
    
    // Check X-Real-IP header (nginx)
    auto xri_it = request.headers.find("x-real-ip");
    if (xri_it != request.headers.end()) {
        return xri_it->second;
    }
    
    // Fall back to socket IP
    return socket_ip;
}

std::string RateLimitMiddleware::extract_user_id(const Claims& claims) {
    // Try "sub" claim (subject - standard JWT claim for user ID)
    if (!claims.sub.empty()) {
        return claims.sub;
    }
    
    // Try "user_id" claim (custom claim)
    auto uid_it = claims.values.find("user_id");
    if (uid_it != claims.values.end()) {
        return uid_it->second;
    }
    
    // No user ID found (unauthenticated request)
    return "";
}

Response RateLimitMiddleware::create_rate_limit_response(size_t retry_after_seconds,
                                                         const std::string& reason) {
    Response response;
    response.status = 429;  // Too Many Requests
    
    // Add Retry-After header (tells client when to retry)
    response.headers["Retry-After"] = std::to_string(retry_after_seconds);
    response.headers["Content-Type"] = "application/json";
    
    // Standard rate limit headers (RateLimit-* are draft standard)
    response.headers["X-RateLimit-Limit"] = "varies";
    response.headers["X-RateLimit-Remaining"] = "0";
    response.headers["X-RateLimit-Reset"] = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + retry_after_seconds);
    
    // JSON body with error details
    nlohmann::json error_body = {
        {"error", "rate_limit_exceeded"},
        {"message", "Too many requests. Please try again later."},
        {"reason", reason},
        {"retry_after", retry_after_seconds}
    };
    
    response.body = error_body.dump(2);
    
    spdlog::info("Created 429 response - reason={}, retry_after={}s", reason, retry_after_seconds);
    
    return response;
}

MultiStrategyRateLimiter::Stats RateLimitMiddleware::get_stats() const {
    return limiter_.get_stats();
}

} // namespace gateway
