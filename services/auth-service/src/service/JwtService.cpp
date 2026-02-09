#include "service/JwtService.hpp"
#include <jwt-cpp/jwt.h>

JwtService::JwtService(std::string issuer,
                       std::string secret,
                       std::chrono::seconds access_ttl,
                       std::chrono::seconds refresh_ttl)
    : issuer_(std::move(issuer)),
      secret_(std::move(secret)),
      access_ttl_(access_ttl),
      refresh_ttl_(refresh_ttl) {}

TokenPair JwtService::issueTokens(const User& user) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();

    const auto access_exp  = now + access_ttl_;
    const auto refresh_exp = now + refresh_ttl_;

    auto access_token = jwt::create()
        .set_type("JWT")
        .set_issuer(issuer_)
        .set_subject(user.id)
        .set_audience("securecloud-client")
        .set_issued_at(now)
        .set_expires_at(access_exp)
        .set_payload_claim("email", jwt::claim(user.email))
        .set_payload_claim("tenant", jwt::claim(user.tenant_id))
        .sign(jwt::algorithm::hs256{secret_});

    auto refresh_token = jwt::create()
        .set_type("JWT")
        .set_issuer(issuer_)
        .set_subject(user.id)
        .set_audience("securecloud-client")
        .set_issued_at(now)
        .set_expires_at(refresh_exp)
        .set_payload_claim("typ", jwt::claim(std::string("refresh")))
        .sign(jwt::algorithm::hs256{secret_});

    return TokenPair{
        .access_token  = std::move(access_token),
        .refresh_token = std::move(refresh_token),
        .access_exp    = access_exp,
        .refresh_exp   = refresh_exp
    };
}
