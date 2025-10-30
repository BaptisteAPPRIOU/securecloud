#include "gateway/authCache.hpp"


namespace gateway {
std::optional<Claims> AuthCache::get(const std::string& jwt) const {
auto it = cache_.find(jwt);
if (it == cache_.end()) return std::nullopt;
return it->second;
}
void AuthCache::put(const std::string& jwt, const Claims& c) { cache_[jwt] = c; }
void AuthCache::evict(const std::string& jwt) { cache_.erase(jwt); }
} // namespace gateway