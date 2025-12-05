#include "authCache.hpp"
#include "gatewayMetrics.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

AuthCache::AuthCache(size_t max_size, int default_ttl_s)
    : max_size_(max_size), default_ttl_s_(default_ttl_s) {
    spdlog::info("AuthCache initialized (max_size: {}, default_ttl: {}s)", 
                 max_size_, default_ttl_s_);
}

std::optional<Claims> AuthCache::get(const std::string& jwt) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(jwt);
    if (it == cache_.end()) {
        miss_count_++;
        GatewayMetrics::instance().record_auth_cache_miss();
        return std::nullopt;
    }

    // Check if entry expired
    if (is_expired(it->second)) {
        spdlog::debug("Cache entry expired for jwt: {}...", jwt.substr(0, 10));
        
        // Remove from cache and LRU
        auto lru_it = lru_map_.find(jwt);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
        cache_.erase(it);
        
        miss_count_++;
        GatewayMetrics::instance().record_auth_cache_miss();
        return std::nullopt;
    }

    // Cache hit - update LRU
    touch_lru(jwt);
    hit_count_++;
    GatewayMetrics::instance().record_auth_cache_hit();
    
    return it->second.claims;
}

void AuthCache::put(const std::string& jwt, const Claims& c, int ttl_s) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Determine TTL
    int ttl = (ttl_s > 0) ? ttl_s : default_ttl_s_;
    auto expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);

    // Check if entry already exists (update case)
    auto it = cache_.find(jwt);
    if (it != cache_.end()) {
        // Update existing entry
        it->second.claims = c;
        it->second.expires_at = expires_at;
        touch_lru(jwt);
        spdlog::debug("Cache entry updated for subject: {}", c.sub);
        return;
    }

    // Check if cache is full - evict LRU if needed
    if (cache_.size() >= max_size_) {
        evict_lru();
    }

    // Insert new entry
    cache_[jwt] = CacheEntry{c, expires_at};
    
    // Add to LRU (front = most recent)
    lru_list_.push_front(jwt);
    lru_map_[jwt] = lru_list_.begin();

    spdlog::debug("Cache entry added for subject: {} (ttl: {}s, cache_size: {})", 
                 c.sub, ttl, cache_.size());
}

void AuthCache::evict(const std::string& jwt) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(jwt);
    if (it != cache_.end()) {
        // Remove from LRU
        auto lru_it = lru_map_.find(jwt);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
        
        // Remove from cache
        cache_.erase(it);
        spdlog::debug("Cache entry evicted for jwt: {}...", jwt.substr(0, 10));
    }
}

void AuthCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    lru_list_.clear();
    lru_map_.clear();
    hit_count_ = 0;
    miss_count_ = 0;
    spdlog::info("AuthCache cleared");
}

std::tuple<size_t, size_t, size_t, size_t> AuthCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {cache_.size(), max_size_, hit_count_, miss_count_};
}

size_t AuthCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t removed = 0;
    auto now = std::chrono::steady_clock::now();
    
    // Iterate through cache and remove expired entries
    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (it->second.expires_at <= now) {
            // Remove from LRU
            auto lru_it = lru_map_.find(it->first);
            if (lru_it != lru_map_.end()) {
                lru_list_.erase(lru_it->second);
                lru_map_.erase(lru_it);
            }
            
            // Remove from cache
            it = cache_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        spdlog::debug("Cleaned up {} expired cache entries", removed);
    }
    
    return removed;
}

void AuthCache::touch_lru(const std::string& jwt) const {
    // Move to front of LRU list (most recently used)
    auto lru_it = lru_map_.find(jwt);
    if (lru_it != lru_map_.end()) {
        // Remove from current position
        lru_list_.erase(lru_it->second);
        
        // Add to front
        lru_list_.push_front(jwt);
        lru_map_[jwt] = lru_list_.begin();
    }
}

void AuthCache::evict_lru() {
    if (lru_list_.empty()) {
        return;
    }
    
    // Get least recently used (back of list)
    const std::string& lru_jwt = lru_list_.back();
    
    // Remove from cache
    cache_.erase(lru_jwt);
    
    // Remove from LRU map
    lru_map_.erase(lru_jwt);
    
    // Remove from LRU list
    lru_list_.pop_back();
    
    spdlog::debug("Evicted LRU cache entry (cache full, size: {})", max_size_);
}

bool AuthCache::is_expired(const CacheEntry& entry) const {
    return entry.expires_at <= std::chrono::steady_clock::now();
}

} // namespace gateway