#include "service/AuthServiceCore.hpp"
#include <stdexcept>

AuthServiceCore::AuthServiceCore(UserRepository& users,
                                 CredentialVerifier& cred,
                                 JwtService& jwt)
    : users_(users), cred_(cred), jwt_(jwt) {}

LoginResult AuthServiceCore::login(const LoginRequest& req) {
    auto userOpt = users_.findByEmail(req.email);
    if (!userOpt.has_value())
        throw std::runtime_error("invalid_credentials");

    const auto& user = *userOpt;
    if (user.status != "enabled")
        throw std::runtime_error("user_disabled");

    if (!cred_.verifyPassword(user, req.password))
        throw std::runtime_error("invalid_credentials");

    // pour l'instant on ignore MFA (tu pourras utiliser user.mfa_required et req.mfa)
    auto tokens = jwt_.issueTokens(user);
    users_.updateLastLogin(user.id);

    return LoginResult{ std::move(tokens), user.mfa_required };
}
