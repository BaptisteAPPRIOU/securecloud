#pragma once

#include "repository/UserRepository.hpp"
#include <chrono>
#include <string>

struct PasswordResetConfig {
    std::string issuer;
    std::string secret;
    std::chrono::seconds token_ttl;
    std::string reset_url_base;
    std::string smtp_host;
    unsigned short smtp_port;
    std::string smtp_from;
};

class PasswordResetService {
public:
    PasswordResetService(UserRepository& users, PasswordResetConfig cfg);

    // Demande un reset : génère un token + envoie (ou log) l'email
    void requestReset(const std::string& email);

    // Applique le reset : vérifie le token + change le mot de passe
    void confirmReset(const std::string& token,
                      const std::string& newPassword);

private:
    UserRepository& users_;
    PasswordResetConfig cfg_;

    void sendEmail(const std::string& to,
                   const std::string& subject,
                   const std::string& body) const;
};
