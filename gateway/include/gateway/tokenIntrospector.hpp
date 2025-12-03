#pragma once
#include "gateway/types.hpp"
#include <optional>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace gateway {

/**
 * JWKS (JSON Web Key Set) cache entry.
 * Stores public keys fetched from auth-service with expiration time.
 */
struct JWKSCacheEntry {
    std::string public_key;  // PEM-encoded RSA/ECDSA public key
    std::chrono::steady_clock::time_point expires_at;
};

/**
 * Token Introspector - JWT verification with JWKS support.
 * 
 * Features:
 * - Local JWT signature verification using jwt-cpp
 * - JWKS fetching and caching from auth-service
 * - Claims extraction (sub, exp, iss, aud, custom claims)
 * - Remote token validation fallback (for revocation checks)
 * 
 * Thread-safety: Thread-safe for concurrent verification requests.
 * 
 * NOTE: Currently uses hardcoded public key for dev.
 * TODO: Implement JWKS HTTP fetching from auth-service endpoint.
 */
class TokenIntrospector {
public:
    /**
     * Construct introspector with optional JWKS config.
     * 
     * @param jwks_url URL to fetch JWKS (e.g., http://auth-service/.well-known/jwks.json)
     * @param cache_ttl_s JWKS cache TTL in seconds (default: 300 = 5 minutes)
     */
    explicit TokenIntrospector(
        const std::string& jwks_url = "",
        int cache_ttl_s = 300
    );

    /**
     * Verify JWT locally using cryptographic signature verification.
     * 
     * Steps:
     * 1. Parse JWT header to extract 'kid' (key ID)
     * 2. Fetch public key from JWKS cache (or load from service)
     * 3. Verify signature using jwt-cpp
     * 4. Validate claims: exp (expiration), iss (issuer), aud (audience)
     * 5. Extract claims (sub, roles, custom fields)
     * 
     * @param jwt JWT token string (without "Bearer " prefix)
     * @return Claims if valid, std::nullopt if invalid or expired
     */
    std::optional<Claims> localVerifyJWT(const std::string& jwt) const;

    /**
     * Validate token remotely via auth-service.
     * Useful for checking token revocation (not detectable locally).
     * 
     * NOTE: Not implemented in current version - reserved for future use.
     * TODO: HTTP POST to auth-service /auth/validate endpoint.
     * 
     * @param jwt JWT token string
     * @return Claims if valid, std::nullopt if revoked or invalid
     */
    std::optional<Claims> remoteValidate(const std::string& jwt) const;

    /**
     * Set hardcoded public key for development/testing.
     * For production, use JWKS endpoint instead.
     * 
     * @param public_key_pem PEM-encoded RSA/ECDSA public key
     */
    void set_dev_public_key(const std::string& public_key_pem);

private:
    std::string jwks_url_;
    int cache_ttl_s_;
    
    // JWKS cache: kid -> (public_key, expiration)
    // Mutable because localVerifyJWT is const but may update cache
    mutable std::unordered_map<std::string, JWKSCacheEntry> jwks_cache_;
    mutable std::mutex cache_mutex_;
    
    // Dev public key (fallback when JWKS not available)
    mutable std::string dev_public_key_;

    /**
     * Fetch JWKS from auth-service and update cache.
     * 
     * TODO: Implement HTTP GET to jwks_url_
     * TODO: Parse JSON response, extract public keys by 'kid'
     * TODO: Convert JWK to PEM format for jwt-cpp
     * 
     * @return true if fetch successful, false otherwise
     */
    bool fetch_jwks() const;

    /**
     * Get public key for JWT verification.
     * Priority: JWKS cache > dev key > fetch from service
     * 
     * @param kid Key ID from JWT header (optional)
     * @return Public key in PEM format, or empty string if not found
     */
    std::string get_public_key(const std::string& kid = "") const;
};

} // namespace gateway