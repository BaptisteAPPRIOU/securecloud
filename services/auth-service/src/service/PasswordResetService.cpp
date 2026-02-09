#include "service/PasswordResetService.hpp"
#include "service/CredentialVerifier.hpp"
#include "service/CryptoService.hpp"

#include <jwt-cpp/jwt.h>
#include <boost/asio.hpp>
#include <iostream>
#include <stdexcept>

namespace net = boost::asio;
using tcp = net::ip::tcp;

PasswordResetService::PasswordResetService(UserRepository& users,
                                           PasswordResetConfig cfg)
    : users_(users),
      cfg_(std::move(cfg)) {}

void PasswordResetService::requestReset(const std::string& email) {
    auto userOpt = users_.findByEmail(email);
    if (!userOpt) {
        // On ne révèle pas si l'utilisateur existe ou pas.
        return;
    }

    const User& user = *userOpt;

    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto exp = now + cfg_.token_ttl;

    // JWT de reset de mot de passe
    auto token = jwt::create()
        .set_issuer(cfg_.issuer)
        .set_audience("securecloud-client")
        .set_subject(user.id)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("typ",   jwt::claim(std::string("password_reset")))
        .set_payload_claim("email", jwt::claim(user.email))
        .sign(jwt::algorithm::hs256{cfg_.secret});

    std::string link = cfg_.reset_url_base + "?token=" + token;

    std::string subject = "Password reset";
    std::string body =
        "Hello " + user.display_name + ",\r\n\r\n"
        "To reset your password, click the following link:\r\n" +
        link +
        "\r\n\r\nIf you did not request this, you can ignore this email.\r\n";

    sendEmail(user.email, subject, body);

    std::cout << "[auth-service] Password reset link for " << user.email
              << ": " << link << "\n";
}

void PasswordResetService::confirmReset(const std::string& token,
                                        const std::string& newPassword) {
    // 1) Décoder le token
    auto decoded = jwt::decode(token);

    // 2) Vérifier la signature / expiry
    auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{cfg_.secret})
        .with_issuer(cfg_.issuer)
        .with_audience("securecloud-client");

    // On convertit toutes les erreurs de vérification en "invalid_token"
    try {
        verifier.verify(decoded);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid_token");
    }

    // 3) Vérifier que c'est bien un token de reset
    std::string typ;
    if (decoded.has_payload_claim("typ")) {
        typ = decoded.get_payload_claim("typ").as_string();
    }
    if (typ != "password_reset") {
        throw std::runtime_error("invalid_token");
    }

    // 4) Récupérer l'email
    if (!decoded.has_payload_claim("email")) {
        throw std::runtime_error("invalid_token");
    }
    std::string email = decoded.get_payload_claim("email").as_string();

    // 5) Retrouver l'utilisateur
    auto userOpt = users_.findByEmail(email);
    if (!userOpt) {
        throw std::runtime_error("user_not_found");
    }

    const User& user = *userOpt;

    // 6) Hash du nouveau mot de passe (même algo que CredentialVerifier)
    std::string new_hash = CryptoService::sha256(newPassword);
    users_.updatePassword(user.id, new_hash, "sha256");
}

void PasswordResetService::sendEmail(const std::string& to,
                                     const std::string& subject,
                                     const std::string& body) const {
    if (cfg_.smtp_host.empty() || cfg_.smtp_port == 0) {
        std::cout << "[auth-service] SMTP not configured; skipping real email send\n";
        std::cout << "[auth-service] Password reset mail to " << to << "\n";
        std::cout << "[auth-service] Body:\n" << body << "\n";
        return;
    }

    try {
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        auto endpoints = resolver.resolve(cfg_.smtp_host,
                                          std::to_string(cfg_.smtp_port));
        tcp::socket socket{ioc};
        net::connect(socket, endpoints);

        auto sendLine = [&](const std::string& line) {
            std::string l = line + "\r\n";
            net::write(socket, net::buffer(l));
        };

        // Version minimale pour MailHog/dev
        sendLine("HELO auth-service");
        sendLine("MAIL FROM:<" + cfg_.smtp_from + ">");
        sendLine("RCPT TO:<" + to + ">");
        sendLine("DATA");
        sendLine("Subject: " + subject + "\r\n"
                 "To: " + to + "\r\n"
                 "Content-Type: text/plain; charset=utf-8\r\n"
                 "\r\n" +
                 body +
                 "\r\n.");
        sendLine("QUIT");
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] SMTP error: " << e.what() << "\n";
        // En dernier recours on ne fait rien, le controller a déjà répondu "ok"
    }
}
