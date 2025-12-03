#include "gateway/upstreamProxy.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <boost/beast/http.hpp>

namespace gateway {

UpstreamProxy::UpstreamProxy(boost::asio::io_context& io_ctx, 
                             const std::vector<UpstreamConfig>& upstreams)
    : io_context_(io_ctx), upstreams_(upstreams) {
    
    // Initialize HTTP connection pool
    http_pool_ = std::make_unique<HttpConnectionPool>(io_context_);
    
    // Pre-create UDS clients for each UDS upstream
    for (const auto& upstream : upstreams_) {
        if (upstream.transport == UpstreamTransport::UDS) {
            auto client = std::make_shared<UdsClient>(io_context_, upstream.address);
            uds_clients_[upstream.name] = client;
            spdlog::info("Created UDS client for upstream={} socket={}", 
                         upstream.name, upstream.address);
        }
    }
    
    spdlog::info("UpstreamProxy initialized with {} upstreams ({} UDS, {} HTTP)",
                 upstreams_.size(), uds_clients_.size(), 
                 upstreams_.size() - uds_clients_.size());
}

std::optional<UpstreamConfig> UpstreamProxy::find_upstream(const std::string& name) const {
    for (const auto& upstream : upstreams_) {
        if (upstream.name == name) {
            return upstream;
        }
    }
    return std::nullopt;
}

Response UpstreamProxy::forward(const Request& r, const std::string& upstream_name) const {
    auto upstream_opt = find_upstream(upstream_name);
    
    if (!upstream_opt) {
        spdlog::error("Upstream not found: {}", upstream_name);
        Response resp;
        resp.status = 502; // Bad Gateway
        resp.body = R"({"error": "upstream_not_found"})";
        resp.headers["Content-Type"] = "application/json";
        return resp;
    }
    
    const auto& upstream = *upstream_opt;
    
    spdlog::debug("Forwarding {} {} to upstream={} transport={} address={}",
                  r.method, r.path, upstream.name, 
                  upstream.transport == UpstreamTransport::UDS ? "UDS" : "TCP",
                  upstream.address);
    
    // Route based on transport type
    if (upstream.transport == UpstreamTransport::UDS) {
        return forward_via_uds(r, upstream);
    } else {
        return forward_via_http(r, upstream);
    }
}

Response UpstreamProxy::forwardHttp(const Request& r, const UpstreamTarget& t) const {
    // Legacy method - redirect to new implementation
    return forward(r, t.name);
}

Response UpstreamProxy::forward_via_http(const Request& r, const UpstreamConfig& upstream) const {
    Response resp;
    
    try {
        // Parse host:port from address
        size_t colon_pos = upstream.address.find(':');
        if (colon_pos == std::string::npos) {
            spdlog::error("Invalid HTTP upstream address: {}", upstream.address);
            resp.status = 502;
            resp.body = R"({"error": "invalid_upstream_address"})";
            return resp;
        }
        
        std::string host = upstream.address.substr(0, colon_pos);
        uint16_t port = std::stoi(upstream.address.substr(colon_pos + 1));
        
        // Get HTTP client from pool
        auto client = http_pool_->get_client(host, port);
        
        // Convert HTTP verb
        boost::beast::http::verb verb = boost::beast::http::verb::get;
        if (r.method == "POST") verb = boost::beast::http::verb::post;
        else if (r.method == "PUT") verb = boost::beast::http::verb::put;
        else if (r.method == "DELETE") verb = boost::beast::http::verb::delete_;
        else if (r.method == "PATCH") verb = boost::beast::http::verb::patch;
        
        // Execute request based on method
        HttpResult result;
        if (r.method == "GET" || r.method == "DELETE") {
            result = client->get(r.path, r.headers);
        } else {
            result = client->post(r.path, r.body, r.headers);
        }
        
        // Convert HttpResult to Response
        resp.status = result.status_code;
        resp.body = result.body;
        resp.headers = result.headers;
        
        if (!result.success) {
            spdlog::warn("HTTP upstream request failed: {}", result.error_message);
            if (resp.status == 0) {
                resp.status = 502; // Bad Gateway
                resp.body = R"({"error": "upstream_connection_failed"})";
            }
        }
        
    } catch (const std::exception& e) {
        spdlog::error("HTTP forwarding exception: {}", e.what());
        resp.status = 502;
        resp.body = R"({"error": "upstream_error"})";
        resp.headers["Content-Type"] = "application/json";
    }
    
    return resp;
}

Response UpstreamProxy::forward_via_uds(const Request& r, const UpstreamConfig& upstream) const {
    Response resp;
    
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Get UDS client
        auto it = uds_clients_.find(upstream.name);
        if (it == uds_clients_.end()) {
            spdlog::error("UDS client not found for upstream: {}", upstream.name);
            resp.status = 502;
            resp.body = R"({"error": "uds_client_not_found"})";
            return resp;
        }
        
        auto& uds_client = it->second;
        
        // Build JSON request for UDS protocol
        nlohmann::json uds_request = {
            {"method", r.method},
            {"path", r.path},
            {"headers", r.headers},
            {"body", r.body}
        };
        
        // Send request via UDS
        auto uds_response_opt = uds_client->send_request(uds_request);
        
        if (!uds_response_opt) {
            spdlog::error("UDS request failed for upstream: {}", upstream.name);
            resp.status = 502;
            resp.body = R"({"error": "uds_request_failed"})";
            return resp;
        }
        
        // Parse UDS response
        const auto& uds_response = *uds_response_opt;
        
        resp.status = uds_response.value("status", 500);
        resp.body = uds_response.value("body", "");
        
        if (uds_response.contains("headers") && uds_response["headers"].is_object()) {
            for (auto& [key, value] : uds_response["headers"].items()) {
                if (value.is_string()) {
                    resp.headers[key] = value.get<std::string>();
                }
            }
        }
        
        spdlog::debug("UDS response: status={} body_size={}", resp.status, resp.body.size());
        
    } catch (const std::exception& e) {
        spdlog::error("UDS forwarding exception: {}", e.what());
        resp.status = 502;
        resp.body = R"({"error": "uds_error"})";
        resp.headers["Content-Type"] = "application/json";
    }
    
    return resp;
}

} // namespace gateway