#include "gateway/tokenIntrospector.hpp"


namespace gateway {
std::optional<Claims> TokenIntrospector::localVerifyJWT(const std::string& jwt) const {
if (jwt == "dev") return Claims{.sub = "dev"};
return std::nullopt;
}


std::optional<Claims> TokenIntrospector::remoteValidate(const std::string& jwt) const {
(void)jwt; // non utilisé dans le kit
return std::nullopt;
}
} // namespace gateway