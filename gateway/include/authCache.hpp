#pragma once
#include "types.hpp"
#include <optional>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>

namespace gateway {

/**
 * Cache entry with expiration time for TTL support.
 */
struct CacheEntry {
    Claims claims;
    std::chrono::steady_clock::time_point expires_at;
};

/**
 * Authentication Cache with TTL and LRU eviction.
 * 
 * Features:
 * - Thread-safe concurrent access
 * - Time-to-live (TTL) for entries
 * - Least Recently Used (LRU) eviction when size limit reached
 * - Automatic cleanup of expired entries
 * 
 * Performance:
 * - O(1) get/put operations (hash map)
 * - O(1) LRU tracking (doubly linked list)
 * 
 * Thread-safety: All methods are thread-safe.
 */
class AuthCache {
public:
    /**
     * Construct cache with size limit and default TTL.
     * 
     * @param max_size Maximum number of cached entries (default: 10000)
     * @param default_ttl_s Default TTL in seconds (default: 300 = 5 minutes)
     */
    explicit AuthCache(size_t max_size = 10000, int default_ttl_s = 300);

    /**
     * Get cached claims for a JWT token.
     * Returns std::nullopt if not found or expired.
     * Updates LRU position on hit.
     * 
     * @param jwt JWT token string (used as cache key)
     * @return Claims if cached and valid, std::nullopt otherwise
     */
    std::optional<Claims> get(const std::string& jwt) const;

    /**
     * Cache claims for a JWT token.
     * Uses default TTL unless overridden.
     * Evicts LRU entry if cache is full.
     * 
     * @param jwt JWT token string (cache key)
     * @param c Claims to cache
     * @param ttl_s TTL override in seconds (optional, uses default if 0)
     */
    void put(const std::string& jwt, const Claims& c, int ttl_s = 0);

    /**
     * Explicitly evict a token from cache.
     * Used for token revocation scenarios.
     * 
     * @param jwt JWT token to evict
     */
    void evict(const std::string& jwt);

    /**
     * Clear all cached entries.
     */
    void clear();

    /**
     * Get cache statistics.
     * 
     * @return Tuple of (current_size, max_size, hit_count, miss_count)
     */
    std::tuple<size_t, size_t, size_t, size_t> stats() const;

    /**
     * Remove expired entries from cache.
     * Called automatically by get() and put(), but can be called manually
     * for periodic cleanup.
     * 
     * @return Number of entries removed
     */
    size_t cleanup_expired();

private:
    size_t max_size_;
    int default_ttl_s_;
    
    // Cache storage: jwt -> (claims, expiration)
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    
    // LRU tracking: list of jwt tokens in access order (most recent at front)
    mutable std::list<std::string> lru_list_;
    
    // Map jwt -> iterator in lru_list_ for O(1) LRU updates
    mutable std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;
    
    // Statistics
    mutable size_t hit_count_{0};
    mutable size_t miss_count_{0};
    
    // Thread safety
    mutable std::mutex mutex_;

    /**
     * Update LRU position for a key (move to front).
     * Must be called with mutex_ held.
     */
    void touch_lru(const std::string& jwt) const;

    /**
     * Evict least recently used entry.
     * Must be called with mutex_ held.
     */
    void evict_lru();

    /**
     * Check if an entry is expired.
     */
    bool is_expired(const CacheEntry& entry) const;
};

} // namespace gateway