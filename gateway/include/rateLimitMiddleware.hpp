#pragma once

#include "rateLimiter.hpp"
#include "types.hpp"
#include <string>
#include <functional>

namespace gateway {

/**
 * Rate Limit Middleware
 * 
 * Applies rate limiting to incoming requests before routing.
 * Returns 429 Too Many Requests if rate limit exceeded.
 * 
 * Usage:
 *   MultiStrategyRateLimiter limiter(config);
 *   RateLimitMiddleware middleware(limiter);
 *   
 *   auto result = middleware.check_rate_limit(request, client_ip, user_id, endpoint);
 *   if (!result.allowed) {
 *       return create_429_response(result.retry_after_seconds);
 *   }
 */
class RateLimitMiddleware {
public:
    explicit RateLimitMiddleware(MultiStrategyRateLimiter& limiter);
    
    /**
     * Check if request should be rate limited
     * @param request HTTP request
     * @param client_ip Client IP address (from socket or X-Forwarded-For)
     * @param user_id User ID from JWT claims (empty if not authenticated)
     * @param endpoint Request path/endpoint
     * @return Rate limit result
     */
    RateLimitResult check_rate_limit(const Request& request,
                                     const std::string& client_ip,
                                     const std::string& user_id,
                                     const std::string& endpoint);
    
    /**
     * Extract client IP from request
     * Checks X-Forwarded-For, X-Real-IP headers first, falls back to socket IP
     */
    static std::string extract_client_ip(const Request& request, const std::string& socket_ip);
    
    /**
     * Extract user ID from JWT claims
     */
    static std::string extract_user_id(const Claims& claims);
    
    /**
     * Create 429 Too Many Requests response
     */
    static Response create_rate_limit_response(size_t retry_after_seconds, const std::string& reason);
    
    /**
     * Get statistics
     */
    MultiStrategyRateLimiter::Stats get_stats() const;

private:
    MultiStrategyRateLimiter& limiter_;
};

} // namespace gateway
