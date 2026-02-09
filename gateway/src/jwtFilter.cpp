#include "jwtFilter.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace gateway {

JwtFilter::JwtFilter(
    std::shared_ptr<TokenIntrospector> introspector,
    std::shared_ptr<AuthCache> cache
) : introspector_(std::move(introspector)), cache_(std::move(cache)) {
    spdlog::debug("JwtFilter initialized with introspector and cache");
}

JwtFilter::JwtFilter() 
    : introspector_(std::make_shared<TokenIntrospector>()), 
      cache_(std::make_shared<AuthCache>()) {
    spdlog::warn("JwtFilter created in DEV mode (no JWKS configuration)");
    
    // For development: accept hardcoded "dev" token
    // In production, this should be removed and real JWKS used
    // TODO: Remove this dev bypass in production
}

std::string JwtFilter::extract_token(const Request& r) const {
    auto it = r.headers.find("authorization");
    if (it == r.headers.end()) {
        // Try lowercase (HTTP headers are case-insensitive)
        it = r.headers.find("Authorization");
        if (it == r.headers.end()) {
            return "";
        }
    }

    const std::string& auth_header = it->second;
    
    // Expected format: "Bearer <token>"
    const std::string bearer_prefix = "Bearer ";
    if (auth_header.size() > bearer_prefix.size() && 
        auth_header.substr(0, bearer_prefix.size()) == bearer_prefix) {
        return auth_header.substr(bearer_prefix.size());
    }

    spdlog::debug("Invalid Authorization header format (expected 'Bearer <token>')");
    return "";
}

std::optional<Claims> JwtFilter::verify(const Request& r) const {
    // Step 1: Extract token from Authorization header
    std::string token = extract_token(r);
    if (token.empty()) {
        spdlog::debug("No JWT token found in request");
        return std::nullopt;
    }

    // DEV MODE BYPASS: Accept "dev" token for development
    // TODO: Remove this in production
    if (token == "dev") {
        spdlog::debug("DEV token accepted (bypass JWT verification)");
        return Claims{.sub = "dev", .values = {{"role", "admin"}}};
    }

    // Step 2: Check cache for previously verified token
    if (cache_) {
        auto cached_claims = cache_->get(token);
        if (cached_claims) {
            spdlog::debug("JWT claims retrieved from cache for subject: {}", cached_claims->sub);
            return cached_claims;
        }
    }

    // Step 3: Verify token using introspector
    if (!introspector_) {
        spdlog::error("TokenIntrospector not initialized");
        return std::nullopt;
    }

    auto claims = introspector_->localVerifyJWT(token);
    if (!claims) {
        spdlog::debug("JWT verification failed");
        return std::nullopt;
    }

    // Step 4: Cache verified claims for future requests
    // TODO: Extract expiration from JWT and set cache TTL accordingly
    if (cache_) {
        cache_->put(token, *claims);
        spdlog::debug("JWT claims cached for subject: {}", claims->sub);
    }

    spdlog::debug("JWT verified successfully for subject: {}", claims->sub);
    return claims;
}

} // namespace gateway