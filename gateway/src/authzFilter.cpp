#include "gateway/authzFilter.hpp"


namespace gateway {
bool AuthzFilter::check(const Claims& c, const std::string&, const std::string& path) const {
if (path == "/v1/healthz") return true; // public
return !c.sub.empty(); // toute identité est acceptée (kit)
}
} // namespace gateway