#pragma once

#include <string>
#include <vector>
#include <optional>
#include "httpServer.hpp"

namespace gateway {

// TLS Configuration
struct TLSConfig {
    std::string cert_file;
    std::string key_file;
    bool client_mtls{false};
    // TODO: Add cipher_suites, min_tls_version for production
};

// Upstream transport types
enum class UpstreamTransport {
    UDS,  // Unix Domain Socket
    TCP   // TCP socket
};

enum class UpstreamKind {
    HTTP,
    WS    // WebSocket
};

// Upstream service definition
struct UpstreamConfig {
    std::string name;
    UpstreamKind kind{UpstreamKind::HTTP};
    UpstreamTransport transport{UpstreamTransport::TCP};
    std::string address;  // UDS path or host:port
    // TODO: Add connection_pool_size, timeout_ms for production
};

// Routing rule
struct RouteConfig {
    std::string match_pattern;  // Regex pattern
    std::string target;          // Upstream name
    bool upgrade_websocket{false};
    // TODO: Add path_rewrite, strip_prefix for advanced routing
};

// JWKS provider configuration
struct JWKSProviderConfig {
    std::string url;
    int cache_ttl_s{300};
};

// Security configuration
struct SecurityConfig {
    std::vector<JWKSProviderConfig> jwks_providers;
    int jwks_cache_ttl_s{300};
    // TODO: Add rate_limit config here
};

// Rate limiting configuration
struct RateLimitConfig {
    bool enabled{true};
    
    // Global limits (entire gateway)
    size_t global_capacity{1000};
    double global_refill_rate{100.0};  // requests/second
    
    // Per-IP limits
    size_t ip_capacity{100};
    double ip_refill_rate{10.0};
    
    // Per-user limits (authenticated)
    size_t user_capacity{500};
    double user_refill_rate{50.0};
    
    // Per-endpoint limits
    size_t endpoint_capacity{200};
    double endpoint_refill_rate{20.0};
};

// Prometheus configuration
struct PrometheusConfig {
    std::string bind_address{"0.0.0.0:9090"};
    bool enabled{true};
};

// Logging configuration
struct LogConfig {
    std::string level{"info"};  // debug, info, warn, error
    std::string format{"json"}; // json or text
    // TODO: Add output_file for persistent logs
};

// Observability configuration
struct ObservabilityConfig {
    PrometheusConfig prometheus;
    LogConfig logs;
};

// Full gateway configuration
struct GatewayConfig {
    ServerConfig server;
    std::optional<TLSConfig> tls;
    std::vector<RouteConfig> routes;
    std::vector<UpstreamConfig> upstreams;
    SecurityConfig security;
    RateLimitConfig rate_limits;
    ObservabilityConfig observability;
    
    // Environment (dev/test/prod) - can be used for feature flags
    std::string environment{"dev"};
};

/**
 * Load complete gateway configuration from YAML file.
 * Parses all sections: server, tls, routing, upstreams, security, observability.
 * Falls back to defaults if file not found or sections missing.
 * 
 * @param path Path to YAML config file (default: config/gateway.dev.yaml)
 * @param env Environment name (dev/test/prod) - can load env-specific files
 * @return Complete gateway configuration
 */
GatewayConfig load_gateway_config(
    const std::string& path = "config/gateway.dev.yaml",
    const std::string& env = "dev"
);

/**
 * DEPRECATED: Legacy function for backward compatibility.
 * Use load_gateway_config() instead.
 */
ServerConfig load_server_config(const std::string& path = "config/gateway.dev.yaml");

}  // namespace gateway
