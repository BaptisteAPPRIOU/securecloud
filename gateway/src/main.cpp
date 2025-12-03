#include "gateway/httpServer.hpp"
#include "gateway/config.hpp"
#include "gateway/router.hpp"
#include "gateway/jwtFilter.hpp"
#include "gateway/authzFilter.hpp"
#include "gateway/upstreamProxy.hpp"
#include "gateway/metrics.hpp"
#include "gateway/auditSink.hpp"
#include <spdlog/spdlog.h>
#include <boost/asio/io_context.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>

using namespace gateway;

int main() {
    spdlog::info("SecureCloud Gateway (Steps 1-6 Complete)");

    // Load complete gateway configuration
    GatewayConfig config = load_gateway_config("config/gateway.dev.yaml", "dev");
    spdlog::info("Server config: {}:{} with {} workers",
                 config.server.host, config.server.port, config.server.thread_pool_size);

    // Create io_context for async operations
    boost::asio::io_context io_ctx;

    // Initialize components with configuration
    Metrics metrics;
    AuditSink audit;
    JwtFilter jwt;
    Router router(config.routes, config.upstreams);
    UpstreamProxy proxy(io_ctx, config.upstreams);
    AuthzFilter authz;
    HttpServer server(config.server);

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

        auto tgt_opt = router.route(r);
        if (!tgt_opt) {
            spdlog::warn("No route found for: {} {}", r.method, r.path);
            return {404, R"({"error":"not_found"})", {}};
        }
        
        const auto& tgt = *tgt_opt;
        audit.record(r, tgt);
        metrics.incCounter("requests_total");
        return proxy.forwardHttp(r, tgt);
    });

    server.start();
    return 0;
}