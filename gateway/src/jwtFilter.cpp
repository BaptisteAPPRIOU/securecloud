#include "jwtFilter.hpp"
#include <jwt-cpp/jwt.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

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
    spdlog::warn("JwtFilter created without explicit configuration");
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

    // Step 2: Check cache for previously verified token
    if (cache_) {
        auto cached_claims = cache_->get(token);
        if (cached_claims) {
            if (introspector_ && introspector_->remote_validation_enabled()) {
                auto remote_claims = introspector_->remoteValidate(token);
                if (!remote_claims) {
                    spdlog::debug("JWT rejected by remote validation");
                    return std::nullopt;
                }
            }
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

    if (introspector_->remote_validation_enabled()) {
        auto remote_claims = introspector_->remoteValidate(token);
        if (!remote_claims) {
            spdlog::debug("JWT rejected by remote validation");
            return std::nullopt;
        }
        if (!remote_claims->sub.empty() && !claims->sub.empty() && remote_claims->sub != claims->sub) {
            spdlog::warn("JWT local/remote subject mismatch: local={} remote={}", claims->sub, remote_claims->sub);
            return std::nullopt;
        }
    }

    // Step 4: Cache verified claims using JWT exp-derived TTL.
    if (cache_) {
        int cache_ttl_s = 0;
        try {
            auto decoded = jwt::decode(token);
            if (decoded.has_expires_at()) {
                const auto now = std::chrono::system_clock::now();
                const auto exp = decoded.get_expires_at();
                if (exp > now) {
                    cache_ttl_s = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::seconds>(exp - now).count()
                    );
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("Unable to derive JWT expiration for cache TTL: {}", e.what());
        }

        if (cache_ttl_s > 0) {
            cache_->put(token, *claims, cache_ttl_s);
            spdlog::debug("JWT claims cached for subject: {} (ttl={}s)", claims->sub, cache_ttl_s);
        } else {
            spdlog::debug("JWT claims not cached for subject: {} (missing/expired exp)", claims->sub);
        }
    }

    spdlog::debug("JWT verified successfully for subject: {}", claims->sub);
    return claims;
}

} // namespace gateway
