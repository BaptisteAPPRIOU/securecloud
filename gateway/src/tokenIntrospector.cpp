#include "tokenIntrospector.hpp"
#include <spdlog/spdlog.h>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>

namespace gateway {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

TokenIntrospector::TokenIntrospector(const std::string& jwt_secret,
                                     const std::string& issuer,
                                     const std::string& audience,
                                     const std::string& auth_service_addr,
                                     const std::string& jwks_url,
                                     int cache_ttl_s)
    : jwt_secret_(jwt_secret),
      issuer_(issuer),
      audience_(audience),
      auth_service_addr_(auth_service_addr),
      jwks_url_(jwks_url),
      cache_ttl_s_(cache_ttl_s) {
    
    if (!jwt_secret_.empty()) {
        spdlog::info("TokenIntrospector initialized with JWT secret for HS256 verification");
    }
    
    if (!jwks_url_.empty()) {
        spdlog::info("TokenIntrospector initialized with JWKS URL: {} (cache TTL: {}s)",
                     jwks_url_, cache_ttl_s_);
    }
    
    if (jwt_secret_.empty() && jwks_url_.empty()) {
        spdlog::warn("TokenIntrospector running in DEV mode (no JWT secret or JWKS URL configured)");
    }

    if (issuer_.empty() || audience_.empty()) {
        spdlog::error("TokenIntrospector requires non-empty JWT issuer and audience");
    }

    if (!auth_service_addr_.empty()) {
        spdlog::info("Remote token validation enabled via auth-service at {}", auth_service_addr_);
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
        
        // Step 2: Extract algorithm from header
        std::string alg = decoded.get_algorithm();
        spdlog::debug("JWT algorithm: {}", alg);
        
        // Step 3: Extract key ID (kid) from header if present
        std::string kid;
        if (decoded.has_header_claim("kid")) {
            kid = decoded.get_header_claim("kid").as_string();
            spdlog::debug("JWT kid: {}", kid);
        }

        // Step 4: Get public key for verification (if using RS256)
        std::string public_key;
        if (alg == "RS256") {
            public_key = get_public_key(kid);
            if (public_key.empty()) {
                spdlog::warn("No public key available for JWT verification (kid: {})", 
                            kid.empty() ? "none" : kid);
                return std::nullopt;
            }
        }

        // Step 5: Verify JWT signature and required claims
        if (issuer_.empty() || audience_.empty()) {
            spdlog::error("JWT verifier misconfigured: issuer or audience is empty");
            return std::nullopt;
        }

        auto verifier_builder = jwt::verify()
            .with_issuer(issuer_)
            .with_audience(audience_);

        if (alg == "HS256") {
            // Symmetric HMAC SHA-256 verification (development/simple deployments)
            if (jwt_secret_.empty()) {
                spdlog::error("JWT uses HS256 but no jwt_secret configured");
                return std::nullopt;
            }
            verifier_builder.allow_algorithm(jwt::algorithm::hs256{jwt_secret_});
            spdlog::debug("Using HS256 verification with shared secret");
        } else if (alg == "RS256") {
            // Asymmetric RSA SHA-256 verification (production with JWKS)
            verifier_builder.allow_algorithm(jwt::algorithm::rs256(public_key));
            spdlog::debug("Using RS256 verification with public key");
        } else {
            spdlog::error("Unsupported JWT algorithm: {}", alg);
            return std::nullopt;
        }

        verifier_builder.verify(decoded);
        spdlog::debug("JWT signature verified successfully");

        // Step 6: Validate expiration
        if (decoded.has_expires_at()) {
            auto exp = decoded.get_expires_at();
            auto now = std::chrono::system_clock::now();
            if (exp < now) {
                spdlog::debug("JWT expired");
                return std::nullopt;
            }
        }

        // Step 6.5: Token type guard (refresh tokens must not authorize API calls).
        if (decoded.has_payload_claim("typ")) {
            const auto typ = decoded.get_payload_claim("typ").as_string();
            if (typ != "access") {
                spdlog::debug("JWT token type is not allowed for API access: {}", typ);
                return std::nullopt;
            }
        }

        // Step 7: Extract claims
        Claims claims;
        
        if (decoded.has_subject()) {
            claims.sub = decoded.get_subject();
        }

        // Extract custom claims if present
        if (decoded.has_payload_claim("email")) {
            claims.values["email"] = decoded.get_payload_claim("email").as_string();
        }
        if (decoded.has_payload_claim("role")) {
            claims.values["role"] = decoded.get_payload_claim("role").as_string();
        }
        if (decoded.has_payload_claim("tenant")) {
            claims.values["tenant"] = decoded.get_payload_claim("tenant").as_string();
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
    if (auth_service_addr_.empty()) {
        spdlog::debug("Remote token validation skipped (auth_service_addr not configured)");
        return std::nullopt;
    }

    try {
        const auto sep = auth_service_addr_.find(':');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= auth_service_addr_.size()) {
            spdlog::error("Invalid auth_service_addr format for remote validation: {}", auth_service_addr_);
            return std::nullopt;
        }

        const std::string host = auth_service_addr_.substr(0, sep);
        const std::string port = auth_service_addr_.substr(sep + 1);

        net::io_context ioc;
        tcp::resolver resolver{ioc};
        beast::tcp_stream stream{ioc};

        auto const results = resolver.resolve(host, port);
        stream.connect(results);

        nlohmann::json payload = {{"token", jwt}};

        http::request<http::string_body> req{http::verb::post, "/auth/validate", 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "SecureCloudGateway/1.0");
        req.set(http::field::content_type, "application/json");
        req.set(http::field::connection, "close");
        req.body() = payload.dump();
        req.prepare_payload();

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        if (res.result_int() != 200) {
            spdlog::debug("Remote token validation rejected with status {}", res.result_int());
            return std::nullopt;
        }

        auto body = nlohmann::json::parse(res.body(), nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            spdlog::warn("Remote token validation returned invalid JSON");
            return std::nullopt;
        }

        if (!body.value("active", false)) {
            spdlog::debug("Remote token validation marked token inactive");
            return std::nullopt;
        }

        Claims claims;
        claims.sub = body.value("sub", "");
        if (body.contains("email") && body["email"].is_string()) {
            claims.values["email"] = body["email"].get<std::string>();
        }
        if (body.contains("tenant") && body["tenant"].is_string()) {
            claims.values["tenant"] = body["tenant"].get<std::string>();
        }
        return claims;
    } catch (const std::exception& e) {
        spdlog::error("Remote token validation failed: {}", e.what());
        return std::nullopt;
    }
}

bool TokenIntrospector::remote_validation_enabled() const {
    return !auth_service_addr_.empty();
}

bool TokenIntrospector::fetch_jwks() const {
    // TODO: Implement JWKS fetching from auth-service
    spdlog::debug("JWKS fetching not implemented (TODO)");
    return false;
}

std::string TokenIntrospector::get_public_key(const std::string& kid) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Priority 1: Check JWKS cache
    if (!kid.empty()) {
        auto it = jwks_cache_.find(kid);
        if (it != jwks_cache_.end()) {
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
    }

    // Priority 3: Use dev public key (development/testing only)
    if (!dev_public_key_.empty()) {
        spdlog::debug("Using dev public key (JWKS not available)");
        return dev_public_key_;
    }

    spdlog::warn("No public key available for JWT verification");
    return "";
}

}
