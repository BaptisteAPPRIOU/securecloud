#include "gateway/jwtFilter.hpp"


namespace gateway {
std::optional<Claims> JwtFilter::verify(const Request& r) const {
auto it = r.headers.find("authorization");
if (it == r.headers.end()) return std::nullopt;
// Kit minimal: accepte "Bearer dev" et renvoie un sub fictif
if (it->second == "Bearer dev") return Claims{.sub = "dev"};
return std::nullopt;
}
} // namespace gateway