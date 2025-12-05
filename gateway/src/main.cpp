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
    GatewayConfig config;
    
    try {
        config = load_gateway_config("config/gateway.dev.yaml", "dev");
    } catch (const std::exception& e) {
        spdlog::critical("Failed to load gateway configuration: {}", e.what());
        spdlog::critical("Please check your configuration file and environment variables.");
        return EXIT_FAILURE;
    }
    
    // Configure logging from config
    spdlog::level::level_enum log_level = parse_log_level(config.observability.logs.level);
    spdlog::set_level(log_level);
    
    spdlog::info("SecureCloud Gateway Starting - Environment: {}", config.environment);
    spdlog::info("Log Level: {}", config.observability.logs.level);
    
    // ===== CONFIGURATION VALIDATION =====
    spdlog::info("Validating gateway configuration...");
    
    // Validate JWT secret is configured
    if (config.security.jwt_secret.empty()) {
        spdlog::critical("JWT secret is not configured!");
        spdlog::critical("Please set the JWT_SECRET environment variable before starting the gateway.");
        spdlog::critical("Example: export JWT_SECRET=your-secret-key");
        return EXIT_FAILURE;
    }
    spdlog::info("✓ JWT secret configured (HS256 symmetric verification enabled)");
    
    // Validate upstream configurations
    for (const auto& upstream : config.upstreams) {
        if (upstream.transport == UpstreamTransport::TCP) {
            // Validate TCP address format (host:port)
            if (upstream.address.find(':') == std::string::npos) {
                spdlog::critical("Invalid TCP upstream address for '{}': {}", 
                               upstream.name, upstream.address);
                spdlog::critical("Expected format: host:port (e.g., localhost:8081)");
                return EXIT_FAILURE;
            }
            spdlog::info("✓ Upstream '{}': TCP transport -> {}", upstream.name, upstream.address);
        } else if (upstream.transport == UpstreamTransport::UDS) {
            // Validate UDS socket path is absolute
            if (upstream.address.empty() || upstream.address[0] != '/') {
                spdlog::critical("Invalid UDS socket path for '{}': {}", 
                               upstream.name, upstream.address);
                spdlog::critical("Expected absolute path (e.g., /run/securecloud/auth.sock)");
                return EXIT_FAILURE;
            }
            spdlog::info("✓ Upstream '{}': UDS transport -> {}", upstream.name, upstream.address);
        }
    }
    
    // Log routing configuration
    spdlog::info("Routing rules configured: {} routes", config.routes.size());
    for (const auto& route : config.routes) {
        spdlog::debug("  Route: {} -> upstream '{}'", route.match_pattern, route.target);
    }
    
    // Log server configuration
    spdlog::info("Server config: {}:{} with {} workers",
                 config.server.host, config.server.port, config.server.thread_pool_size);
    
    spdlog::info("✓ Configuration validation complete");
    // ===== END VALIDATION =====

    // Create io_context for async operations
    boost::asio::io_context io_ctx;

    // Initialize components with configuration
    Metrics metrics;
    AuditSink audit;
    
    // Initialize JWT verification with configured secret
    auto introspector = std::make_shared<TokenIntrospector>(config.security.jwt_secret);
    auto auth_cache = std::make_shared<AuthCache>();
    JwtFilter jwt(introspector, auth_cache);
    
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