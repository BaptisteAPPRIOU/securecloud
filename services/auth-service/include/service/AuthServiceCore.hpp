#pragma once
#include "repository/UserRepository.hpp"
#include "service/CredentialVerifier.hpp"
#include "service/JwtService.hpp"
#include <optional>
#include <string>

struct LoginRequest {
    std::string email;
    std::string password;
    std::optional<std::string> mfa; // pour plus tard
};

struct LoginResult {
    TokenPair tokens;
    bool mfa_required;
};

class AuthServiceCore {
public:
    AuthServiceCore(UserRepository& users,
                    CredentialVerifier& cred,
                    JwtService& jwt);

    LoginResult login(const LoginRequest& req);

private:
    UserRepository& users_;
    CredentialVerifier& cred_;
    JwtService& jwt_;
};
