#include "httpServer.hpp"
#include "config.hpp"
#include "router.hpp"
#include "jwtFilter.hpp"
#include "authzFilter.hpp"
#include "upstreamProxy.hpp"
#include "metrics.hpp"
#include "auditSink.hpp"
#include <spdlog/spdlog.h>
#include <boost/asio/io_context.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>

using namespace gateway;

// Parse log level from string
spdlog::level::level_enum parse_log_level(const std::string& level) {
    std::string level_lower = level;
    std::transform(level_lower.begin(), level_lower.end(), level_lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    if (level_lower == "trace") return spdlog::level::trace;
    if (level_lower == "debug") return spdlog::level::debug;
    if (level_lower == "info") return spdlog::level::info;
    if (level_lower == "warn" || level_lower == "warning") return spdlog::level::warn;
    if (level_lower == "error") return spdlog::level::err;
    if (level_lower == "critical") return spdlog::level::critical;
    if (level_lower == "off") return spdlog::level::off;
    
    return spdlog::level::info; // Default
}

int main() {
    // Load complete gateway configuration
    GatewayConfig config = load_gateway_config("config/gateway.dev.yaml", "dev");
    
    // Configure logging from config
    spdlog::level::level_enum log_level = parse_log_level(config.observability.logs.level);
    spdlog::set_level(log_level);
    
    spdlog::info("SecureCloud Gateway (Steps 1-9 Complete) - Log Level: {}", 
                 config.observability.logs.level);
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