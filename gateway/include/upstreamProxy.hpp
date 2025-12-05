#pragma once

#include "types.hpp"
#include "config.hpp"
#include "httpClient.hpp"
#include "udsClient.hpp"
#include <boost/asio/io_context.hpp>
#include <memory>

namespace gateway {

/**
 * Upstream Proxy - forwards requests to microservices
 * 
 * Features:
 * - HTTP forwarding via TCP or Unix Domain Sockets
 * - WebSocket forwarding for real-time communication
 * - Automatic transport selection based on UpstreamConfig
 * - Connection pooling for performance
 * - Error handling and retry logic
 */
class UpstreamProxy {
public:
    UpstreamProxy(boost::asio::io_context& io_ctx, const std::vector<UpstreamConfig>& upstreams);
    
    /**
     * Forward HTTP request to upstream service
     * @param r Client request
     * @param t Upstream target
     * @return Response from upstream service
     */
    Response forwardHttp(const Request& r, const UpstreamTarget& t) const;
    
    /**
     * Forward HTTP request using upstream name lookup
     * @param r Client request
     * @param upstream_name Name of upstream service (e.g., "auth-service")
     * @return Response from upstream service
     */
    Response forward(const Request& r, const std::string& upstream_name) const;

private:
    // Internal forwarding methods
    Response forward_via_http(const Request& r, const UpstreamConfig& upstream) const;
    Response forward_via_uds(const Request& r, const UpstreamConfig& upstream) const;
    
    // Helper to find upstream config by name
    std::optional<UpstreamConfig> find_upstream(const std::string& name) const;
    
    boost::asio::io_context& io_context_;
    std::vector<UpstreamConfig> upstreams_;
    
    // Connection pools
    mutable std::unique_ptr<HttpConnectionPool> http_pool_;
    mutable std::unordered_map<std::string, std::shared_ptr<UdsClient>> uds_clients_;
    mutable std::mutex mutex_;
};

} // namespace gateway