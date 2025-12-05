#include <gtest/gtest.h>
#include "rateLimiter.hpp"
#include <thread>
#include <chrono>

using namespace gateway;

TEST(TokenBucketTest, AllowsRequestsWithinCapacity) {
    TokenBucket bucket(10, 10.0);  // 10 capacity, 10 tokens/second
    
    // Should allow 10 requests immediately
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(bucket.try_consume(1));
    }
    
    // 11th request should be rate limited
    EXPECT_FALSE(bucket.try_consume(1));
}

TEST(TokenBucketTest, RefillsOverTime) {
    TokenBucket bucket(10, 10.0);  // Refills at 10 tokens/second
    
    // Consume all tokens
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(bucket.try_consume(1));
    }
    EXPECT_FALSE(bucket.try_consume(1));  // Should be limited
    
    // Wait 100ms (should refill ~1 token)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should allow 1 request after refill
    EXPECT_TRUE(bucket.try_consume(1));
}

TEST(TokenBucketTest, ResetRestoresCapacity) {
    TokenBucket bucket(5, 5.0);
    
    // Consume all tokens
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(bucket.try_consume(1));
    }
    EXPECT_FALSE(bucket.try_consume(1));
    
    // Reset should restore full capacity
    bucket.reset();
    EXPECT_TRUE(bucket.try_consume(1));
}

TEST(RateLimiterTest, AllowsRequestsByKey) {
    RateLimiter limiter(10, 10.0);
    
    // Different keys should have independent buckets
    EXPECT_TRUE(limiter.allow("user1"));
    EXPECT_TRUE(limiter.allow("user2"));
    EXPECT_TRUE(limiter.allow("user1"));
}

TEST(RateLimiterTest, CustomLimitsPerKey) {
    RateLimiter limiter(10, 10.0);  // Default: 10 capacity, 10/s
    
    // Set custom limit for specific key
    limiter.set_limit("premium_user", 100, 100.0);
    
    // Premium user should have higher limit
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(limiter.allow("premium_user"));
    }
    EXPECT_FALSE(limiter.allow("premium_user"));  // 101st should fail
    
    // Regular key should have lower limit
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(limiter.allow("regular_user"));
    }
    EXPECT_FALSE(limiter.allow("regular_user"));
}

TEST(RateLimiterTest, BucketCleanup) {
    RateLimiter limiter(10, 10.0);
    
    limiter.allow("temp_user1");
    limiter.allow("temp_user2");
    limiter.allow("temp_user3");
    
    EXPECT_EQ(limiter.get_bucket_count(), 3);
    
    // Wait slightly to ensure buckets have idle time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Cleanup with 0 second idle time should remove all buckets
    limiter.cleanup(std::chrono::seconds(0));
    
    // All buckets should be removed (they've been idle > 0 seconds)
    EXPECT_EQ(limiter.get_bucket_count(), 0);
}

TEST(MultiStrategyRateLimiterTest, GlobalLimit) {
    RateLimitConfig config;
    config.global_capacity = 5;
    config.global_refill_rate = 5.0;
    config.ip_capacity = 100;
    config.user_capacity = 100;
    config.endpoint_capacity = 100;
    
    MultiStrategyRateLimiter limiter(config);
    
    // Should allow 5 global requests
    for (int i = 0; i < 5; i++) {
        auto result = limiter.check("192.168.1.1", "", "/api/test");
        EXPECT_TRUE(result.allowed);
    }
    
    // 6th request should be rate limited globally
    auto result = limiter.check("192.168.1.2", "", "/api/test");  // Different IP
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "global_rate_limit_exceeded");
}

TEST(MultiStrategyRateLimiterTest, IPLimit) {
    RateLimitConfig config;
    config.global_capacity = 1000;
    config.ip_capacity = 3;  // Limit per IP
    config.ip_refill_rate = 3.0;
    
    MultiStrategyRateLimiter limiter(config);
    
    // IP 1: Should allow 3 requests
    for (int i = 0; i < 3; i++) {
        auto result = limiter.check("192.168.1.1", "", "/api/test");
        EXPECT_TRUE(result.allowed);
    }
    
    // IP 1: 4th request should be rate limited
    auto result = limiter.check("192.168.1.1", "", "/api/test");
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "ip_rate_limit_exceeded");
    
    // IP 2: Should still be allowed (different bucket)
    result = limiter.check("192.168.1.2", "", "/api/test");
    EXPECT_TRUE(result.allowed);
}

TEST(MultiStrategyRateLimiterTest, UserLimit) {
    RateLimitConfig config;
    config.global_capacity = 1000;
    config.ip_capacity = 1000;
    config.user_capacity = 2;  // Limit per user
    config.user_refill_rate = 2.0;
    
    MultiStrategyRateLimiter limiter(config);
    
    // User 1: Should allow 2 requests
    for (int i = 0; i < 2; i++) {
        auto result = limiter.check("192.168.1.1", "user1", "/api/test");
        EXPECT_TRUE(result.allowed);
    }
    
    // User 1: 3rd request should be rate limited
    auto result = limiter.check("192.168.1.1", "user1", "/api/test");
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "user_rate_limit_exceeded");
}

TEST(MultiStrategyRateLimiterTest, EndpointLimit) {
    RateLimitConfig config;
    config.global_capacity = 1000;
    config.ip_capacity = 1000;
    config.user_capacity = 1000;
    config.endpoint_capacity = 3;  // Limit per endpoint
    config.endpoint_refill_rate = 3.0;
    
    MultiStrategyRateLimiter limiter(config);
    
    // Endpoint 1: Should allow 3 requests
    for (int i = 0; i < 3; i++) {
        auto result = limiter.check("192.168.1.1", "", "/api/limited");
        EXPECT_TRUE(result.allowed);
    }
    
    // Endpoint 1: 4th request should be rate limited
    auto result = limiter.check("192.168.1.1", "", "/api/limited");
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.reason, "endpoint_rate_limit_exceeded");
    
    // Different endpoint: Should be allowed
    result = limiter.check("192.168.1.1", "", "/api/other");
    EXPECT_TRUE(result.allowed);
}

TEST(MultiStrategyRateLimiterTest, DisabledRateLimiting) {
    RateLimitConfig config;
    config.enabled = false;  // Disable rate limiting
    
    MultiStrategyRateLimiter limiter(config);
    
    // Should allow unlimited requests when disabled
    for (int i = 0; i < 1000; i++) {
        auto result = limiter.check("192.168.1.1", "user1", "/api/test");
        EXPECT_TRUE(result.allowed);
    }
}

TEST(MultiStrategyRateLimiterTest, Statistics) {
    RateLimitConfig config;
    config.global_capacity = 2;
    config.ip_capacity = 2;
    
    MultiStrategyRateLimiter limiter(config);
    
    // Trigger global limit
    limiter.check("192.168.1.1", "", "/api/test");
    limiter.check("192.168.1.2", "", "/api/test");
    limiter.check("192.168.1.3", "", "/api/test");  // Should hit global limit
    
    auto stats = limiter.get_stats();
    EXPECT_EQ(stats.global_requests, 1);
    EXPECT_GT(stats.total_buckets, 0);
}
