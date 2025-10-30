#include "gateway/httpServer.hpp"
#include "gateway/router.hpp"
#include "gateway/jwtFilter.hpp"
#include "gateway/authzFilter.hpp"
#include "gateway/upstreamProxy.hpp"
#include "gateway/metrics.hpp"
#include "gateway/auditSink.hpp"
#include <spdlog/spdlog.h>


using namespace gateway;


int main() {
spdlog::info("SecureCloud Gateway (kit minimal)");


Metrics metrics;
AuditSink audit;
JwtFilter jwt;
Router router;
UpstreamProxy proxy;
AuthzFilter authz;
HttpServer server;


server.onRequest([&](const Request& r) -> Response {
if (r.path == "/v1/healthz") return {200, "ok"};


auto claims = jwt.verify(r);
if (!claims) return {401, "unauthorized"};
if (!authz.check(*claims, r.method, r.path)) return {403, "forbidden"};


auto tgt = router.route(r);
audit.record(r, tgt);
metrics.incCounter("requests_total");
return proxy.forwardHttp(r, tgt);
});


server.start();
return 0;
}