#include "gateway/upstreamProxy.hpp"
#include <fmt/core.h>


namespace gateway {
Response UpstreamProxy::forwardHttp(const Request& r, const UpstreamTarget& t) const {
Response resp;
resp.status = 200;
resp.body = fmt::format("[kit] proxied {} {} to target:{} addr:{}", r.method, r.path, t.name, t.address);
return resp;
}
} // namespace gateway