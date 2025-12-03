#include "gateway/tokenIntrospector.hpp"
#include <spdlog/spdlog.h>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

namespace gateway {

TokenIntrospector::TokenIntrospector(const std::string& jwks_url, int cache_ttl_s)
    : jwks_url_(jwks_url), cache_ttl_s_(cache_ttl_s) {
    
    if (!jwks_url_.empty()) {
        spdlog::info("TokenIntrospector initialized with JWKS URL: {} (cache TTL: {}s)",
                     jwks_url_, cache_ttl_s_);
    } else {
        spdlog::warn("TokenIntrospector running in DEV mode (no JWKS URL configured)");
    }
}

void TokenIntrospector::set_dev_public_key(const std::string& public_key_pem) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    dev_public_key_ = public_key_pem;
    spdlog::debug("Dev public key configured ({} bytes)", public_key_pem.size());
}

std::optional<Claims> TokenIntrospector::localVerifyJWT(const std::string& jwt) const {
    try {
        // Step 1: Decode JWT without verification to extract header
        auto decoded = jwt::decode(jwt);
        
        // Step 2: Extract key ID (kid) from header if present
        std::string kid;
        if (decoded.has_header_claim("kid")) {
            kid = decoded.get_header_claim("kid").as_string();
            spdlog::debug("JWT kid: {}", kid);
        }

        // Step 3: Get public key for verification
        std::string public_key = get_public_key(kid);
        if (public_key.empty()) {
            spdlog::warn("No public key available for JWT verification (kid: {})", 
                        kid.empty() ? "none" : kid);
            return std::nullopt;
        }

        // Step 4: Verify JWT signature
        // Determine algorithm from JWT header
        std::string alg = decoded.get_algorithm();
        spdlog::debug("JWT algorithm: {}", alg);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(public_key))  // RSA SHA-256
            // TODO: Support other algorithms (ES256, RS512, etc.)
            .with_issuer("securecloud-auth")  // TODO: Make configurable
            // .with_audience("gateway")  // Uncomment if needed
            ;

        verifier.verify(decoded);
        spdlog::debug("JWT signature verified successfully");

        // Step 5: Validate expiration
        if (decoded.has_expires_at()) {
            auto exp = decoded.get_expires_at();
            auto now = std::chrono::system_clock::now();
            if (exp < now) {
                spdlog::debug("JWT expired");
                return std::nullopt;
            }
        }

        // Step 6: Extract claims
        Claims claims;
        
        if (decoded.has_subject()) {
            claims.sub = decoded.get_subject();
        }

        // Extract custom claims (roles, permissions, etc.)
        if (decoded.has_payload_claim("role")) {
            claims.values["role"] = decoded.get_payload_claim("role").as_string();
        }
        if (decoded.has_payload_claim("permissions")) {
            // TODO: Handle array claims properly
            // For now, store as JSON string
            claims.values["permissions"] = decoded.get_payload_claim("permissions").to_json().serialize();
        }
        if (decoded.has_payload_claim("email")) {
            claims.values["email"] = decoded.get_payload_claim("email").as_string();
        }

        spdlog::debug("JWT verified for subject: {}", claims.sub);
        return claims;

    } catch (const jwt::error::token_verification_exception& e) {
        spdlog::warn("JWT verification failed: {}", e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::error("JWT parsing error: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Claims> TokenIntrospector::remoteValidate(const std::string& jwt) const {
    // TODO: Implement remote validation via auth-service
    // This is useful for checking token revocation (cannot be detected locally)
    // 
    // Steps:
    // 1. HTTP POST to auth-service /auth/validate endpoint
    // 2. Send JWT in request body (JSON: {"token": "..."})  
    // 3. Parse response: {"valid": true/false, "claims": {...}}
    // 4. Return claims if valid
    // 
    // NOTE: Protocol can be JSON over IPC (UDS or TCP) instead of HTTP
    //       See upstreamProxy for IPC client implementation.
    
    (void)jwt;
    spdlog::debug("Remote token validation not implemented (TODO)");
    return std::nullopt;
}

bool TokenIntrospector::fetch_jwks() const {
    // TODO: Implement JWKS fetching from auth-service
    // 
    // Steps:
    // 1. HTTP GET to jwks_url_ (e.g., http://auth-service/.well-known/jwks.json)
    // 2. Parse JSON response: {"keys": [{"kid": "...", "kty": "RSA", "n": "...", "e": "..."}]}
    // 3. Convert JWK to PEM format using jwt-cpp utilities
    // 4. Store in jwks_cache_ with expiration time
    // 
    // NOTE: Can use Boost.Beast HTTP client or custom IPC client
    //       See httpClient.cpp for HTTP implementation (Step 4)
    // 
    // NOTE: For now, using Protocol Buffers/MessagePack could optimize this,
    //       but JSON is sufficient for JWKS (infrequent fetch, small payload)
    
    spdlog::debug("JWKS fetching not implemented (TODO)");
    return false;
}

std::string TokenIntrospector::get_public_key(const std::string& kid) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Priority 1: Check JWKS cache
    if (!kid.empty()) {
        auto it = jwks_cache_.find(kid);
        if (it != jwks_cache_.end()) {
            // Check if cache entry expired
            auto now = std::chrono::steady_clock::now();
            if (it->second.expires_at > now) {
                spdlog::debug("Using cached JWKS key (kid: {})", kid);
                return it->second.public_key;
            } else {
                spdlog::debug("JWKS cache entry expired (kid: {})", kid);
                jwks_cache_.erase(it);
            }
        }
    }

    // Priority 2: Fetch from JWKS service (if configured)
    if (!jwks_url_.empty()) {
        // TODO: Call fetch_jwks() and retry cache lookup
        // For now, fall through to dev key
    }

    // Priority 3: Use dev public key (development/testing only)
    if (!dev_public_key_.empty()) {
        spdlog::debug("Using dev public key (JWKS not available)");
        return dev_public_key_;
    }

    spdlog::warn("No public key available for JWT verification");
    return "";
}

} // namespace gateway