#include "gatewayMetrics.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

// ============================================================================
// GatewayMetrics Implementation
// ============================================================================

GatewayMetrics& GatewayMetrics::instance() {
    static GatewayMetrics metrics;
    return metrics;
}

void GatewayMetrics::initialize() {
    auto& registry = MetricsRegistry::instance();
    
    // Request metrics
    requests_total_ = registry.register_counter(
        "gateway_requests_total",
        "Total number of HTTP requests received by the gateway"
    );
    
    responses_total_ = registry.register_counter(
        "gateway_responses_total",
        "Total number of HTTP responses sent by the gateway"
    );
    
    request_duration_ = registry.register_histogram(
        "gateway_request_duration_seconds",
        "Request duration in seconds",
        {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0}
    );
    
    request_errors_total_ = registry.register_counter(
        "gateway_request_errors_total",
        "Total number of request errors"
    );
    
    // Connection metrics
    active_connections_ = registry.register_gauge(
        "gateway_active_connections",
        "Number of currently active client connections"
    );
    
    // Rate limiting metrics
    rate_limit_total_ = registry.register_counter(
        "gateway_rate_limit_total",
        "Total number of rate-limited requests"
    );
    
    // Upstream metrics
    upstream_requests_total_ = registry.register_counter(
        "gateway_upstream_requests_total",
        "Total number of requests forwarded to upstream services"
    );
    
    upstream_responses_total_ = registry.register_counter(
        "gateway_upstream_responses_total",
        "Total number of responses received from upstream services"
    );
    
    upstream_duration_ = registry.register_histogram(
        "gateway_upstream_duration_seconds",
        "Upstream request duration in seconds",
        {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0}
    );
    
    upstream_errors_total_ = registry.register_counter(
        "gateway_upstream_errors_total",
        "Total number of upstream errors"
    );
    
    // Router metrics
    route_matches_total_ = registry.register_counter(
        "gateway_route_matches_total",
        "Total number of successful route matches"
    );
    
    route_misses_total_ = registry.register_counter(
        "gateway_route_misses_total",
        "Total number of failed route matches"
    );
    
    // Authentication metrics
    auth_cache_hits_total_ = registry.register_counter(
        "gateway_auth_cache_hits_total",
        "Total number of authentication cache hits"
    );
    
    auth_cache_misses_total_ = registry.register_counter(
        "gateway_auth_cache_misses_total",
        "Total number of authentication cache misses"
    );
    
    jwt_validations_total_ = registry.register_counter(
        "gateway_jwt_validations_total",
        "Total number of JWT validations"
    );
    
    spdlog::info("Gateway metrics initialized");
}

void GatewayMetrics::record_request(const std::string& method, const std::string& path) {
    if (requests_total_) {
        requests_total_->increment({{"method", method}});
    }
}

void GatewayMetrics::record_response(const std::string& method, int status_code, double duration_seconds) {
    if (!responses_total_ || !request_duration_) return;
    
    std::string status_class;
    if (status_code >= 200 && status_code < 300) status_class = "2xx";
    else if (status_code >= 300 && status_code < 400) status_class = "3xx";
    else if (status_code >= 400 && status_code < 500) status_class = "4xx";
    else if (status_code >= 500 && status_code < 600) status_class = "5xx";
    else status_class = "other";
    
    responses_total_->increment({
        {"method", method},
        {"status", std::to_string(status_code)},
        {"status_class", status_class}
    });
    
    request_duration_->observe({{"method", method}}, duration_seconds);
}

void GatewayMetrics::record_request_error(const std::string& error_type) {
    if (request_errors_total_) {
        request_errors_total_->increment({{"error_type", error_type}});
    }
}

void GatewayMetrics::increment_active_connections() {
    if (active_connections_) {
        active_connections_->increment();
    }
}

void GatewayMetrics::decrement_active_connections() {
    if (active_connections_) {
        active_connections_->decrement();
    }
}

void GatewayMetrics::record_rate_limit(const std::string& strategy) {
    if (rate_limit_total_) {
        rate_limit_total_->increment({{"strategy", strategy}});
    }
}

void GatewayMetrics::record_upstream_request(const std::string& upstream_name, 
                                             const std::string& transport_type) {
    if (upstream_requests_total_) {
        upstream_requests_total_->increment({
            {"upstream", upstream_name},
            {"transport", transport_type}
        });
    }
}

void GatewayMetrics::record_upstream_response(const std::string& upstream_name, 
                                              int status_code,
                                              double duration_seconds) {
    if (!upstream_responses_total_ || !upstream_duration_) return;
    
    std::string status_class;
    if (status_code >= 200 && status_code < 300) status_class = "2xx";
    else if (status_code >= 300 && status_code < 400) status_class = "3xx";
    else if (status_code >= 400 && status_code < 500) status_class = "4xx";
    else if (status_code >= 500 && status_code < 600) status_class = "5xx";
    else status_class = "other";
    
    upstream_responses_total_->increment({
        {"upstream", upstream_name},
        {"status", std::to_string(status_code)},
        {"status_class", status_class}
    });
    
    upstream_duration_->observe({{"upstream", upstream_name}}, duration_seconds);
}

void GatewayMetrics::record_upstream_error(const std::string& upstream_name,
                                           const std::string& error_type) {
    if (upstream_errors_total_) {
        upstream_errors_total_->increment({
            {"upstream", upstream_name},
            {"error_type", error_type}
        });
    }
}

void GatewayMetrics::record_route_match(const std::string& pattern) {
    if (route_matches_total_) {
        route_matches_total_->increment({{"pattern", pattern}});
    }
}

void GatewayMetrics::record_route_miss() {
    if (route_misses_total_) {
        route_misses_total_->increment();
    }
}

void GatewayMetrics::record_auth_cache_hit() {
    if (auth_cache_hits_total_) {
        auth_cache_hits_total_->increment();
    }
}

void GatewayMetrics::record_auth_cache_miss() {
    if (auth_cache_misses_total_) {
        auth_cache_misses_total_->increment();
    }
}

void GatewayMetrics::record_jwt_validation(bool success) {
    if (jwt_validations_total_) {
        jwt_validations_total_->increment({{"result", success ? "success" : "failure"}});
    }
}

// ============================================================================
// RequestMetricsTracker Implementation
// ============================================================================

RequestMetricsTracker::RequestMetricsTracker(const std::string& method, const std::string& path)
    : method_(method), path_(path), 
      start_(std::chrono::steady_clock::now()), 
      recorded_(false) {
    
    GatewayMetrics::instance().record_request(method_, path_);
    GatewayMetrics::instance().increment_active_connections();
}

RequestMetricsTracker::~RequestMetricsTracker() {
    if (!recorded_) {
        // If not explicitly recorded, record as error
        record_error("unhandled");
    }
    
    GatewayMetrics::instance().decrement_active_connections();
}

void RequestMetricsTracker::record_response(int status_code) {
    if (recorded_) return;
    
    auto end = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(end - start_).count();
    
    GatewayMetrics::instance().record_response(method_, status_code, duration);
    recorded_ = true;
}

void RequestMetricsTracker::record_error(const std::string& error_type) {
    if (recorded_) return;
    
    GatewayMetrics::instance().record_request_error(error_type);
    recorded_ = true;
}

} // namespace gateway
