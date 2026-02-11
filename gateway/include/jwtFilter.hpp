#pragma once
#include "types.hpp"
#include "tokenIntrospector.hpp"
#include "authCache.hpp"
#include <optional>
#include <memory>

namespace gateway {

/**
 * JWT Authentication Filter
 * 
 * Extracts and verifies JWT tokens from HTTP Authorization header.
 * Uses TokenIntrospector for cryptographic verification and AuthCache for performance.
 * 
 * Flow:
 * 1. Extract "Authorization: Bearer <token>" header
 * 2. Check AuthCache for previously verified token
 * 3. If not cached, verify using TokenIntrospector
 * 4. Cache verified claims for subsequent requests
 * 5. Return Claims or std::nullopt (triggers 401 Unauthorized)
 * 
 * Thread-safety: Thread-safe for concurrent requests.
 */
class JwtFilter {
public:
    /**
     * Construct JWT filter with introspector and cache.
     * 
     * @param introspector Token verification engine (shared)
     * @param cache Claims cache for performance (shared)
     */
    JwtFilter(
        std::shared_ptr<TokenIntrospector> introspector,
        std::shared_ptr<AuthCache> cache
    );

    /**
     * Default constructor for backward compatibility.
     * Creates filter components with default initialization.
     */
    JwtFilter();

    /**
     * Verify JWT token from request.
     * 
     * @param r HTTP request with headers
     * @return Claims if token valid, std::nullopt if missing/invalid (triggers 401)
     */
    std::optional<Claims> verify(const Request& r) const;

private:
    std::shared_ptr<TokenIntrospector> introspector_;
    std::shared_ptr<AuthCache> cache_;

    /**
     * Extract JWT token from Authorization header.
     * Expected format: "Authorization: Bearer <token>"
     * 
     * @param r HTTP request
     * @return Token string (without "Bearer " prefix), or empty if not found
     */
    std::string extract_token(const Request& r) const;
};

} // namespace gateway
