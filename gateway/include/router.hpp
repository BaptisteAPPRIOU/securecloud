#pragma once

#include "types.hpp"
#include "config.hpp"
#include <vector>
#include <regex>
#include <optional>

namespace gateway {

/**
 * Route matcher for dynamic routing based on configuration
 * 
 * Features:
 * - Regex pattern matching for flexible route definitions
 * - First-match routing (order matters in config)
 * - WebSocket upgrade detection
 * - Path parameter extraction (future enhancement)
 * - Route priority support
 */
struct RouteMatcher {
    RouteConfig config;
    std::regex pattern;
    
    RouteMatcher(const RouteConfig& cfg) : config(cfg), pattern(cfg.match_pattern) {}
    
    bool matches(const std::string& path) const {
        return std::regex_match(path, pattern);
    }
};

/**
 * Router - maps incoming requests to upstream services
 * 
 * Features:
 * - Configuration-driven routing (from YAML)
 * - Regex-based path matching
 * - WebSocket routing support
 * - Fallback to default route
 */
class Router {
public:
    /**
     * Constructor with route configuration
     * @param routes List of route configurations from YAML
     * @param upstreams List of upstream configurations
     */
    Router(const std::vector<RouteConfig>& routes, 
           const std::vector<UpstreamConfig>& upstreams);
    
    Router() = default;
    
    /**
     * Route request to appropriate upstream target
     * @param r Incoming request
     * @return Upstream target or nullopt if no route matches
     */
    std::optional<UpstreamTarget> route(const Request& r) const;
    
    /**
     * Check if request should upgrade to WebSocket
     * @param r Request to check
     * @return true if route is configured for WebSocket upgrade
     */
    bool should_upgrade_websocket(const Request& r) const;
    
    /**
     * Get total number of configured routes
     */
    size_t get_route_count() const { return route_matchers_.size(); }

private:
    std::vector<RouteMatcher> route_matchers_;
    std::vector<UpstreamConfig> upstreams_;
    
    // Helper to find upstream config by name
    std::optional<UpstreamConfig> find_upstream(const std::string& name) const;
};

} // namespace gateway