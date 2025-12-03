#include "gateway/router.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

Router::Router(const std::vector<RouteConfig>& routes, 
               const std::vector<UpstreamConfig>& upstreams)
    : upstreams_(upstreams) {
    
    // Compile regex patterns for all routes
    for (const auto& route : routes) {
        try {
            route_matchers_.emplace_back(route);
            spdlog::info("Route registered: pattern='{}' -> upstream='{}' ws={}",
                         route.match_pattern, route.target, route.upgrade_websocket);
        } catch (const std::regex_error& e) {
            spdlog::error("Invalid regex pattern '{}': {}", route.match_pattern, e.what());
        }
    }
    
    spdlog::info("Router initialized with {} routes", route_matchers_.size());
}

std::optional<UpstreamConfig> Router::find_upstream(const std::string& name) const {
    for (const auto& upstream : upstreams_) {
        if (upstream.name == name) {
            return upstream;
        }
    }
    return std::nullopt;
}

std::optional<UpstreamTarget> Router::route(const Request& r) const {
    spdlog::debug("Routing request: {} {}", r.method, r.path);
    
    // Try to match each route in order (first match wins)
    for (const auto& matcher : route_matchers_) {
        if (matcher.matches(r.path)) {
            auto upstream_opt = find_upstream(matcher.config.target);
            
            if (!upstream_opt) {
                spdlog::error("Route matched but upstream not found: target='{}'", 
                             matcher.config.target);
                continue;
            }
            
            const auto& upstream = *upstream_opt;
            
            UpstreamTarget target;
            target.name = upstream.name;
            target.address = upstream.address;
            target.websocket = matcher.config.upgrade_websocket;
            
            spdlog::debug("Route matched: pattern='{}' -> upstream='{}' address='{}' ws={}",
                         matcher.config.match_pattern, target.name, target.address, 
                         target.websocket);
            
            return target;
        }
    }
    
    spdlog::warn("No route matched for: {} {}", r.method, r.path);
    return std::nullopt;
}

bool Router::should_upgrade_websocket(const Request& r) const {
    for (const auto& matcher : route_matchers_) {
        if (matcher.matches(r.path)) {
            return matcher.config.upgrade_websocket;
        }
    }
    return false;
}

} // namespace gateway