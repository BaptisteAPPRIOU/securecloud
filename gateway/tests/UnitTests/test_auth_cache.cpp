#include <gtest/gtest.h>
#include "authCache.hpp"
#include <thread>
#include <chrono>

using namespace gateway;

TEST(AuthCacheTest, PutAndGet) {
    AuthCache cache(100, 60);
    
    Claims claims{.sub = "user123", .values = {{"role", "admin"}}};
    cache.put("token1", claims);
    
    auto result = cache.get("token1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->sub, "user123");
    EXPECT_EQ(result->values.at("role"), "admin");
}

TEST(AuthCacheTest, GetNonExistent) {
    AuthCache cache;
    
    auto result = cache.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(AuthCacheTest, Eviction) {
    AuthCache cache(100, 60);
    
    Claims claims{.sub = "user123", .values = {}};
    cache.put("token1", claims);
    
    auto result1 = cache.get("token1");
    EXPECT_TRUE(result1.has_value());
    
    cache.evict("token1");
    
    auto result2 = cache.get("token1");
    EXPECT_FALSE(result2.has_value());
}

TEST(AuthCacheTest, TTLExpiration) {
    AuthCache cache(100, 1);  // 1 second TTL
    
    Claims claims{.sub = "user123", .values = {}};
    cache.put("token1", claims);
    
    // Should be valid immediately
    auto result1 = cache.get("token1");
    EXPECT_TRUE(result1.has_value());
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Should be expired
    auto result2 = cache.get("token1");
    EXPECT_FALSE(result2.has_value());
}

TEST(AuthCacheTest, LRUEviction) {
    AuthCache cache(3, 60);  // Max 3 entries
    
    Claims c1{.sub = "user1", .values = {}};
    Claims c2{.sub = "user2", .values = {}};
    Claims c3{.sub = "user3", .values = {}};
    Claims c4{.sub = "user4", .values = {}};
    
    cache.put("token1", c1);
    cache.put("token2", c2);
    cache.put("token3", c3);
    
    // Access token1 to make it most recently used
    cache.get("token1");
    
    // Add token4 - should evict token2 (least recently used)
    cache.put("token4", c4);
    
    EXPECT_TRUE(cache.get("token1").has_value());   // Still there
    EXPECT_FALSE(cache.get("token2").has_value());  // Evicted (LRU)
    EXPECT_TRUE(cache.get("token3").has_value());   // Still there
    EXPECT_TRUE(cache.get("token4").has_value());   // Newly added
}

TEST(AuthCacheTest, Stats) {
    AuthCache cache(100, 60);
    
    Claims claims{.sub = "user123", .values = {}};
    cache.put("token1", claims);
    
    cache.get("token1");  // Hit
    cache.get("token2");  // Miss
    cache.get("token1");  // Hit
    
    auto [size, max_size, hits, misses] = cache.stats();
    
    EXPECT_EQ(size, 1);
    EXPECT_EQ(max_size, 100);
    EXPECT_EQ(hits, 2);
    EXPECT_EQ(misses, 1);
}

TEST(AuthCacheTest, Clear) {
    AuthCache cache;
    
    Claims claims{.sub = "user123", .values = {}};
    cache.put("token1", claims);
    cache.put("token2", claims);
    
    cache.clear();
    
    auto [size, _, __, ___] = cache.stats();
    EXPECT_EQ(size, 0);
    EXPECT_FALSE(cache.get("token1").has_value());
    EXPECT_FALSE(cache.get("token2").has_value());
}

TEST(AuthCacheTest, ThreadSafety) {
    AuthCache cache(1000, 60);
    
    // Launch multiple threads doing concurrent put/get
    auto worker = [&cache](int thread_id) {
        for (int i = 0; i < 100; i++) {
            std::string token = "token_" + std::to_string(thread_id) + "_" + std::to_string(i);
            Claims claims{.sub = "user" + std::to_string(i), .values = {}};
            
            cache.put(token, claims);
            cache.get(token);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Should not crash (basic thread safety check)
    SUCCEED();
}
