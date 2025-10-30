#include "gateway/router.hpp"


namespace gateway {
UpstreamTarget Router::route(const Request& r) const {
if (r.path.rfind("/v1/auth/", 0) == 0) {
return {"auth", "/run/securecloud/auth.sock", false};
}
if (r.path == "/v1/rt/connect") {
return {"messaging", "messaging:8081", true};
}
return {"files", "/run/securecloud/files.sock", false};
}
} // namespace gateway