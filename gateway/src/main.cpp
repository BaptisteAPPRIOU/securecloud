#include "gateway/httpServer.hpp"
#include "gateway/config.hpp"
#include "gateway/router.hpp"
#include "gateway/jwtFilter.hpp"
#include "gateway/authzFilter.hpp"
#include "gateway/upstreamProxy.hpp"
#include "gateway/metrics.hpp"
#include "gateway/auditSink.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cctype>

using namespace gateway;

int main() {
    spdlog::info("SecureCloud Gateway (kit minimal)");

    ServerConfig server_config = load_server_config();
    spdlog::info("Server config: {}:{} with {} workers",
                 server_config.host, server_config.port, server_config.thread_pool_size);

    Metrics metrics;
    AuditSink audit;
    JwtFilter jwt;
    Router router;
    UpstreamProxy proxy;
    AuthzFilter authz;
    HttpServer server(server_config);

    server.onRequest([&](const Request& r) -> Response {
        if (r.path == "/v1/healthz") {
            return {200, R"({"status":"ok"})", {}};
        }

        auto claims = jwt.verify(r);
        if (!claims) {
            return {401, R"({"error":"unauthorized"})", {}};
        }
        
        if (!authz.check(*claims, r.method, r.path)) {
            return {403, R"({"error":"forbidden"})", {}};
        }

        auto tgt = router.route(r);
        audit.record(r, tgt);
        metrics.incCounter("requests_total");
        return proxy.forwardHttp(r, tgt);
    });

    server.start();
    return 0;
}