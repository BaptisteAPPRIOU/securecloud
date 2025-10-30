#pragma once
#include "gateway/types.hpp"
#include <optional>
#include <string>


namespace gateway {
class TokenIntrospector {
public:
std::optional<Claims> localVerifyJWT(const std::string& jwt) const;
std::optional<Claims> remoteValidate(const std::string& jwt) const;
};
} // namespace gateway