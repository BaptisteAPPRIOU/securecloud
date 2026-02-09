#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>

namespace gateway {

/**
 * Token Bucket Rate Limiter
 * 
 * Algorithm:
 * - Each bucket has a capacity (max tokens) and refill rate (tokens/second)
 * - Tokens are added continuously at the refill rate
 * - Each request consumes 1 token
 * - If no tokens available, request is rate limited
 * 
 * Features:
 * - Per-key rate limiting (IP, user, endpoint)
 * - Configurable capacity and refill rate
 * - Automatic token refill based on elapsed time
 * - Thread-safe with mutex protection
 */
class TokenBucket {
public:
    /**
     * Constructor
     * @param capacity Maximum number of tokens (burst size)
     * @param refill_rate Tokens added per second
     */
    TokenBucket(size_t capacity, double refill_rate);
    
    /**
     * Try to consume tokens
     * @param tokens Number of tokens to consume (default: 1)
     * @return true if tokens were available and consumed, false if rate limited
     */
    bool try_consume(size_t tokens = 1);
    
    /**
     * Get current token count (for metrics)
     */
    double get_tokens() const;
    
    /**
     * Reset bucket to full capacity
     */
    void reset();

private:
    void refill();
    
    size_t capacity_;
    double refill_rate_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    mutable std::mutex mutex_;
};

/**
 * Rate Limiter with multiple buckets
 * 
 * Supports different limiting strategies:
 * - IP-based: Limit requests per IP address
 * - User-based: Limit requests per authenticated user
 * - Endpoint-based: Limit requests per API endpoint
 * - Global: Overall rate limit for entire gateway
 */
class RateLimiter {
public:
    /**
     * Constructor
     * @param capacity Default bucket capacity
     * @param refill_rate Default refill rate (requests/second)
     */
    RateLimiter(size_t capacity = 100, double refill_rate = 10.0);
    
    /**
     * Check if request is allowed for given key
     * @param key Rate limit key (IP, user_id, endpoint)
     * @param tokens Number of tokens to consume
     * @return true if allowed, false if rate limited
     */
    bool allow(const std::string& key, size_t tokens = 1);
    
    /**
     * Set custom rate limit for specific key
     * @param key Rate limit key
     * @param capacity Bucket capacity
     * @param refill_rate Refill rate
     */
    void set_limit(const std::string& key, size_t capacity, double refill_rate);
    
    /**
     * Get bucket for key (creates if not exists)
     */
    std::shared_ptr<TokenBucket> get_bucket(const std::string& key);
    
    /**
     * Get statistics
     */
    size_t get_bucket_count() const;
    
    /**
     * Clean up old buckets (not used recently)
     */
    void cleanup(std::chrono::seconds max_idle = std::chrono::seconds(300));

private:
    struct BucketInfo {
        std::shared_ptr<TokenBucket> bucket;
        std::chrono::steady_clock::time_point last_access;
    };
    
    size_t default_capacity_;
    double default_refill_rate_;
    
    std::unordered_map<std::string, BucketInfo> buckets_;
    std::unordered_map<std::string, std::pair<size_t, double>> custom_limits_;
    mutable std::mutex mutex_;
};

/**
 * Rate Limit Configuration
 */
struct RateLimitConfig {
    bool enabled{true};
    
    // Global limits
    size_t global_capacity{1000};
    double global_refill_rate{100.0};  // requests/second
    
    // Per-IP limits
    size_t ip_capacity{100};
    double ip_refill_rate{10.0};
    
    // Per-user limits (authenticated)
    size_t user_capacity{500};
    double user_refill_rate{50.0};
    
    // Per-endpoint limits (can be overridden per route)
    size_t endpoint_capacity{200};
    double endpoint_refill_rate{20.0};
};

/**
 * Rate Limit Result
 */
struct RateLimitResult {
    bool allowed{true};
    std::string reason;
    size_t retry_after_seconds{0};  // For Retry-After header
};

/**
 * Multi-Strategy Rate Limiter
 * 
 * Applies multiple rate limiting strategies:
 * 1. Global rate limit (entire gateway)
 * 2. IP-based rate limit
 * 3. User-based rate limit (if authenticated)
 * 4. Endpoint-based rate limit
 */
class MultiStrategyRateLimiter {
public:
    MultiStrategyRateLimiter(const RateLimitConfig& config);
    
    /**
     * Check if request is allowed
     * @param ip_address Client IP address
     * @param user_id User ID (empty if not authenticated)
     * @param endpoint Endpoint path
     * @return Rate limit result
     */
    RateLimitResult check(const std::string& ip_address,
                         const std::string& user_id,
                         const std::string& endpoint);
    
    /**
     * Update configuration
     */
    void update_config(const RateLimitConfig& config);
    
    /**
     * Get statistics
     */
    struct Stats {
        size_t total_buckets;
        size_t global_requests;
        size_t ip_limited;
        size_t user_limited;
        size_t endpoint_limited;
    };
    
    Stats get_stats() const;

private:
    RateLimitConfig config_;
    
    RateLimiter global_limiter_;
    RateLimiter ip_limiter_;
    RateLimiter user_limiter_;
    RateLimiter endpoint_limiter_;
    
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

} // namespace gateway
