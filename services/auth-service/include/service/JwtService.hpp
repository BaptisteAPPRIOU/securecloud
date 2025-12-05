#pragma once
#include "domain/User.hpp"
#include <string>
#include <chrono>

struct TokenPair {
    std::string access_token;
    std::string refresh_token;
    std::chrono::system_clock::time_point access_exp;
    std::chrono::system_clock::time_point refresh_exp;
};

class JwtService {
public:
    JwtService(std::string issuer,
               std::string secret,
               std::chrono::seconds access_ttl,
               std::chrono::seconds refresh_ttl);

    TokenPair issueTokens(const User& user);

private:
    std::string issuer_;
    std::string secret_;
    std::chrono::seconds access_ttl_;
    std::chrono::seconds refresh_ttl_;
};
