#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "repository/UserRepository.hpp"
#include "service/CredentialVerifier.hpp"
#include "service/JwtService.hpp"
#include "service/AuthServiceCore.hpp"
#include "service/PasswordResetService.hpp"
#include "service/ProfileService.hpp"
#include "web/AuthController.hpp"

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;

// --- Helpers lecture env ---

static std::string env(const char* name, const char* def) {
    if (const char* v = std::getenv(name)) {
        return v;
    }
    return def;
}

static std::chrono::seconds env_seconds(const char* name, const char* def) {
    int v = std::atoi(env(name, def).c_str());
    if (v < 0) v = 0;
    return std::chrono::seconds{v};
}

// --- Routing HTTP ---

static void handle_request(const http::request<http::string_body>& req,
                           AuthController& controller,
                           http::response<http::string_body>& res) {
    if (req.target() == "/auth/login") {
        res = controller.handleLogin(req);
    } else if (req.target() == "/auth/reset-password") {
        res = controller.handleRequestPasswordReset(req);
    } else if (req.target() == "/auth/reset-password/confirm") {
        res = controller.handleConfirmPasswordReset(req);
    } else if (req.target() == "/auth/logout") {
        res = controller.handleLogout(req);
    } else if (req.target() == "/auth/refresh") {
        res = controller.handleRefresh(req);
    } else if (req.target() == "/auth/profile") {
        res = controller.handleProfile(req);
    } else if (req.target() == "/auth/mfa/verify") { 
        res = controller.handleMfaVerify(req); 
    } else {
        res.version(req.version());
        res.keep_alive(req.keep_alive());
        res.result(http::status::not_found);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"not_found"})";
        res.prepare_payload();
    }
}

static void do_session(tcp::socket socket, AuthController& controller) {
    try {
        beast::flat_buffer buffer;

        for (;;) {
            http::request<http::string_body> req;
            beast::error_code ec;

            http::read(socket, buffer, req, ec);
            if (ec == http::error::end_of_stream) {
                break;
            }
            if (ec) {
                std::cerr << "[auth-service] read error: " << ec.message() << "\n";
                return;
            }

            http::response<http::string_body> res;
            handle_request(req, controller, res);

            bool keep_alive = res.keep_alive();

            http::write(socket, res, ec);
            if (ec) {
                std::cerr << "[auth-service] write error: " << ec.message() << "\n";
                return;
            }

            if (!keep_alive) {
                break;
            }
        }

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] session exception: " << e.what() << "\n";
    }
}

// --- main() ---

int main() {
    try {
        // --- Config DB ---
        std::string db_host = env("DB_HOST", "127.0.0.1");
        std::string db_port = env("DB_PORT", "15432");   // override via env si besoin
        std::string db_name = env("DB_NAME", "securecloud_dev");
        std::string db_user = env("DB_USER", "securecloud");
        std::string db_pass = env("DB_PASS", "securecloud");

        const std::string conninfo =
            "host="      + db_host +
            " port="     + db_port +
            " dbname="   + db_name +
            " user="     + db_user +
            " password=" + db_pass;

        // --- Config JWT ---
        auto jwt_secret = env("JWT_SECRET", "dev-secret-a-changer");
        auto jwt_issuer = env("JWT_ISSUER", "securecloud-auth");

        std::chrono::seconds access_ttl  = env_seconds("JWT_ACCESS_TTL", "900");      // 15 min
        std::chrono::seconds refresh_ttl = env_seconds("JWT_REFRESH_TTL", "604800");  // 7 jours

        // TTL spécifique pour le token de reset de mot de passe
        std::chrono::seconds reset_ttl   = env_seconds("RESET_TOKEN_TTL", "3600");    // 1 h

        // --- Config HTTP ---
        std::string bind_addr = env("AUTH_BIND_ADDR", "0.0.0.0");
        unsigned short port = static_cast<unsigned short>(
            std::atoi(env("AUTH_PORT", "8081").c_str())
        );

        // --- Config SMTP + reset URL ---
        std::string smtp_host = env("SMTP_HOST", "");
        unsigned short smtp_port = static_cast<unsigned short>(
            std::atoi(env("SMTP_PORT", "0").c_str())
        );
        std::string smtp_from = env("SMTP_FROM", "no-reply@demo.local");
        std::string reset_url = env("RESET_PASSWORD_URL",
                                    "https://securecloud.local/reset-password");

        // --- Wiring des services ---

        UserRepository     userRepo{conninfo};
        CredentialVerifier credVerifier;

        JwtService jwtService{
            jwt_issuer,
            jwt_secret,
            access_ttl,
            refresh_ttl
        };

        PasswordResetConfig prCfg{
            .issuer         = jwt_issuer,
            .secret         = jwt_secret,
            .token_ttl      = reset_ttl,
            .reset_url_base = reset_url,
            .smtp_host      = smtp_host,
            .smtp_port      = smtp_port,
            .smtp_from      = smtp_from
        };

        PasswordResetService passwordReset{userRepo, prCfg};
        ProfileService       profileService{userRepo};
        AuthServiceCore      authCore{userRepo, credVerifier, jwtService};

        AuthController controller{authCore, passwordReset, profileService};

        // --- Serveur HTTP ---
        net::io_context ioc{1};
        tcp::endpoint endpoint{net::ip::make_address(bind_addr), port};
        tcp::acceptor acceptor{ioc, endpoint};

        std::cout << "auth-service listening on " << bind_addr << ":" << port << "\n";

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread{do_session, std::move(socket), std::ref(controller)}.detach();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] fatal error: " << e.what() << "\n";
        return 1;
    }
}