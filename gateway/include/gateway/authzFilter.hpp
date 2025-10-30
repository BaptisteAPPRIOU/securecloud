#pragma once
#include "gateway/types.hpp"


namespace gateway {
class AuthzFilter {
public:
bool check(const Claims& c, const std::string& method, const std::string& path) const;
};
} // namespace gateway