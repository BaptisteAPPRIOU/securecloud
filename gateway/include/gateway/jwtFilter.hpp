#pragma once
#include "gateway/types.hpp"
#include <optional>


namespace gateway {
class JwtFilter {
public:
JwtFilter() = default;
std::optional<Claims> verify(const Request& r) const; // 401 si nullopt
};
} // namespace gateway