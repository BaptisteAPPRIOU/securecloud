#include "service/CredentialVerifier.hpp"
#include "service/CryptoService.hpp"

#include <spdlog/spdlog.h>

bool CredentialVerifier::verifyPassword(const User& user,
                                        const std::string& candidate) const
{
    // Cas 1 : mot de passe en clair (utile pour les comptes de démo / données seed)
    if (user.password_algo == "plain" || user.password_algo.empty()) {
        return candidate == user.pass_hash;
    }

    // Cas 2 : mot de passe hashé en SHA-256
    if (user.password_algo == "sha256") {
        const auto hashed = CryptoService::sha256(candidate);
        return hashed == user.pass_hash;
    }

    // Algo inconnu → on loggue et on refuse
    spdlog::warn("[auth-service] Unknown password_algo '{}' for user '{}'",
                 user.password_algo, user.email);
    return false;
}
