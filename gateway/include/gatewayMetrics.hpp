#pragma once

#include "prometheusExporter.hpp"
#include <memory>
#include <string>

namespace gateway {

/**
 * Central metrics manager for the gateway
 * 
 * Provides singleton access to all gateway metrics:
 * - Request counters (total, by method, by status)
 * - Request duration histograms
 * - Active connections gauge
 * - Rate limit counters
 * - Upstream request metrics
 * - Error counters
 */
class GatewayMetrics {
public:
    static GatewayMetrics& instance();
    
    // Initialize all metrics (call once at startup)
    void initialize();
    
    // Request metrics
    void record_request(const std::string& method, const std::string& path);
    void record_response(const std::string& method, int status_code, double duration_seconds);
    void record_request_error(const std::string& error_type);
    
    // Connection metrics
    void increment_active_connections();
    void decrement_active_connections();
    
    // Rate limiting metrics
    void record_rate_limit(const std::string& strategy);  // global, ip, user, endpoint
    
    // Upstream metrics
    void record_upstream_request(const std::string& upstream_name, 
                                 const std::string& transport_type);
    void record_upstream_response(const std::string& upstream_name, 
                                  int status_code, 
                                  double duration_seconds);
    void record_upstream_error(const std::string& upstream_name, 
                              const std::string& error_type);
    
    // Router metrics
    void record_route_match(const std::string& pattern);
    void record_route_miss();
    
    // Authentication metrics
    void record_auth_cache_hit();
    void record_auth_cache_miss();
    void record_jwt_validation(bool success);
    
    // Get individual metrics for custom operations
    std::shared_ptr<Counter> get_requests_total() { return requests_total_; }
    std::shared_ptr<Histogram> get_request_duration() { return request_duration_; }
    std::shared_ptr<Gauge> get_active_connections() { return active_connections_; }

private:
    GatewayMetrics() = default;
    
    // Request metrics
    std::shared_ptr<Counter> requests_total_;
    std::shared_ptr<Counter> responses_total_;
    std::shared_ptr<Histogram> request_duration_;
    std::shared_ptr<Counter> request_errors_total_;
    
    // Connection metrics
    std::shared_ptr<Gauge> active_connections_;
    
    // Rate limiting metrics
    std::shared_ptr<Counter> rate_limit_total_;
    
    // Upstream metrics
    std::shared_ptr<Counter> upstream_requests_total_;
    std::shared_ptr<Counter> upstream_responses_total_;
    std::shared_ptr<Histogram> upstream_duration_;
    std::shared_ptr<Counter> upstream_errors_total_;
    
    // Router metrics
    std::shared_ptr<Counter> route_matches_total_;
    std::shared_ptr<Counter> route_misses_total_;
    
    // Authentication metrics
    std::shared_ptr<Counter> auth_cache_hits_total_;
    std::shared_ptr<Counter> auth_cache_misses_total_;
    std::shared_ptr<Counter> jwt_validations_total_;
};

/**
 * RAII helper for tracking request duration and active connections
 * 
 * Usage:
 *   {
 *       RequestMetricsTracker tracker(method, path);
 *       // ... handle request ...
 *       tracker.record_response(status_code);
 *   } // automatically records duration and decrements active connections
 */
class RequestMetricsTracker {
public:
    RequestMetricsTracker(const std::string& method, const std::string& path);
    ~RequestMetricsTracker();
    
    void record_response(int status_code);
    void record_error(const std::string& error_type);

private:
    std::string method_;
    std::string path_;
    std::chrono::steady_clock::time_point start_;
    bool recorded_;
};

} // namespace gateway
