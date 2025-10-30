#pragma once
#include "gateway/types.hpp"
#include <optional>
#include <unordered_map>


namespace gateway {
class AuthCache {
public:
std::optional<Claims> get(const std::string& jwt) const;
void put(const std::string& jwt, const Claims& c);
void evict(const std::string& jwt);
private:
mutable std::unordered_map<std::string, Claims> cache_;
};
} // namespace gateway