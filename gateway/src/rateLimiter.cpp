#include "rateLimiter.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace gateway {

// ============================================================================
// TokenBucket Implementation
// ============================================================================

TokenBucket::TokenBucket(size_t capacity, double refill_rate)
    : capacity_(capacity), refill_rate_(refill_rate), tokens_(capacity),
      last_refill_(std::chrono::steady_clock::now()) {
    spdlog::debug("TokenBucket created: capacity={}, refill_rate={}/s", capacity, refill_rate);
}

void TokenBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
    
    double tokens_to_add = elapsed * refill_rate_;
    tokens_ = std::min(static_cast<double>(capacity_), tokens_ + tokens_to_add);
    
    last_refill_ = now;
}

bool TokenBucket::try_consume(size_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    refill();
    
    if (tokens_ >= tokens) {
        tokens_ -= tokens;
        return true;
    }
    
    return false;
}

double TokenBucket::get_tokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tokens_;
}

void TokenBucket::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = capacity_;
    last_refill_ = std::chrono::steady_clock::now();
}

// ============================================================================
// RateLimiter Implementation
// ============================================================================

RateLimiter::RateLimiter(size_t capacity, double refill_rate)
    : default_capacity_(capacity), default_refill_rate_(refill_rate) {
    spdlog::info("RateLimiter initialized: default_capacity={}, default_rate={}/s",
                 capacity, refill_rate);
}

bool RateLimiter::allow(const std::string& key, size_t tokens) {
    auto bucket = get_bucket(key);
    bool allowed = bucket->try_consume(tokens);
    
    if (!allowed) {
        spdlog::debug("Rate limit exceeded for key: {}", key);
    }
    
    return allowed;
}

void RateLimiter::set_limit(const std::string& key, size_t capacity, double refill_rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_limits_[key] = {capacity, refill_rate};
    
    // Remove existing bucket so it will be recreated with new limits
    buckets_.erase(key);
    
    spdlog::info("Set custom rate limit for key={}: capacity={}, rate={}/s",
                 key, capacity, refill_rate);
}

std::shared_ptr<TokenBucket> RateLimiter::get_bucket(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = buckets_.find(key);
    if (it != buckets_.end()) {
        it->second.last_access = std::chrono::steady_clock::now();
        return it->second.bucket;
    }
    
    // Create new bucket with custom or default limits
    size_t capacity = default_capacity_;
    double refill_rate = default_refill_rate_;
    
    auto custom_it = custom_limits_.find(key);
    if (custom_it != custom_limits_.end()) {
        capacity = custom_it->second.first;
        refill_rate = custom_it->second.second;
    }
    
    auto bucket = std::make_shared<TokenBucket>(capacity, refill_rate);
    buckets_[key] = {bucket, std::chrono::steady_clock::now()};
    
    spdlog::debug("Created new bucket for key={}: capacity={}, rate={}/s",
                  key, capacity, refill_rate);
    
    return bucket;
}

size_t RateLimiter::get_bucket_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buckets_.size();
}

void RateLimiter::cleanup(std::chrono::seconds max_idle) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    size_t removed = 0;
    
    for (auto it = buckets_.begin(); it != buckets_.end(); ) {
        auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_access);
        
        if (idle_time > max_idle) {
            it = buckets_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        spdlog::info("Cleaned up {} idle rate limit buckets", removed);
    }
}

// ============================================================================
// MultiStrategyRateLimiter Implementation
// ============================================================================

MultiStrategyRateLimiter::MultiStrategyRateLimiter(const RateLimitConfig& config)
    : config_(config),
      global_limiter_(config.global_capacity, config.global_refill_rate),
      ip_limiter_(config.ip_capacity, config.ip_refill_rate),
      user_limiter_(config.user_capacity, config.user_refill_rate),
      endpoint_limiter_(config.endpoint_capacity, config.endpoint_refill_rate) {
    
    spdlog::info("MultiStrategyRateLimiter initialized - enabled={}", config.enabled);
}

RateLimitResult MultiStrategyRateLimiter::check(const std::string& ip_address,
                                                const std::string& user_id,
                                                const std::string& endpoint) {
    if (!config_.enabled) {
        return {true, "", 0};
    }
    
    RateLimitResult result;
    
    // 1. Global rate limit (entire gateway)
    if (!global_limiter_.allow("global")) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.global_requests++;
        
        result.allowed = false;
        result.reason = "global_rate_limit_exceeded";
        result.retry_after_seconds = 1;
        
        spdlog::warn("Global rate limit exceeded");
        return result;
    }
    
    // 2. IP-based rate limit
    if (!ip_limiter_.allow(ip_address)) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.ip_limited++;
        
        result.allowed = false;
        result.reason = "ip_rate_limit_exceeded";
        result.retry_after_seconds = static_cast<size_t>(
            config_.ip_capacity / config_.ip_refill_rate);
        
        spdlog::warn("IP rate limit exceeded for: {}", ip_address);
        return result;
    }
    
    // 3. User-based rate limit (if authenticated)
    if (!user_id.empty() && !user_limiter_.allow(user_id)) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.user_limited++;
        
        result.allowed = false;
        result.reason = "user_rate_limit_exceeded";
        result.retry_after_seconds = static_cast<size_t>(
            config_.user_capacity / config_.user_refill_rate);
        
        spdlog::warn("User rate limit exceeded for: {}", user_id);
        return result;
    }
    
    // 4. Endpoint-based rate limit
    if (!endpoint_limiter_.allow(endpoint)) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.endpoint_limited++;
        
        result.allowed = false;
        result.reason = "endpoint_rate_limit_exceeded";
        result.retry_after_seconds = static_cast<size_t>(
            config_.endpoint_capacity / config_.endpoint_refill_rate);
        
        spdlog::warn("Endpoint rate limit exceeded for: {}", endpoint);
        return result;
    }
    
    // All checks passed
    return {true, "", 0};
}

void MultiStrategyRateLimiter::update_config(const RateLimitConfig& config) {
    config_ = config;
    spdlog::info("Rate limiter configuration updated - enabled={}", config.enabled);
}

MultiStrategyRateLimiter::Stats MultiStrategyRateLimiter::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    Stats current_stats = stats_;
    current_stats.total_buckets = 
        global_limiter_.get_bucket_count() +
        ip_limiter_.get_bucket_count() +
        user_limiter_.get_bucket_count() +
        endpoint_limiter_.get_bucket_count();
    
    return current_stats;
}

} // namespace gateway
