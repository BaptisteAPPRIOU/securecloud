#include "web/AuthController.hpp"

#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <cstdlib>  
#include <jwt-cpp/jwt.h>

using json = nlohmann::json;
namespace http = boost::beast::http;

AuthController::AuthController(AuthServiceCore& core,
                               PasswordResetService& passwordReset,
                               ProfileService& profile)
    : core_(core),
      passwordReset_(passwordReset),
      profile_(profile) {}

// -----------------------------------------------------------------------------
// /auth/login
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleLogin(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.prepare_payload();
            return res;
        }

        auto body = json::parse(req.body());

        LoginRequest login;
        login.email    = body.at("email").get<std::string>();
        login.password = body.at("password").get<std::string>();
        if (body.contains("mfa") && !body["mfa"].is_null()) {
            login.mfa = body["mfa"].get<std::string>();
        }

        auto result = core_.login(login);

        auto to_unix = [](std::chrono::system_clock::time_point tp) {
            return std::chrono::system_clock::to_time_t(tp);
        };

        json out = {
            {"access_token",  result.tokens.access_token},
            {"refresh_token", result.tokens.refresh_token},
            {"access_exp",    to_unix(result.tokens.access_exp)},
            {"refresh_exp",   to_unix(result.tokens.refresh_exp)},
            {"mfa_required",  result.mfa_required},
            {"token_type",    "Bearer"}
        };

        res.body() = out.dump();
        res.prepare_payload();
        return res;
    }
    catch (const json::exception&) {
        // Erreur de JSON d'entrée
        res.result(http::status::bad_request);
        res.body() = R"({"error":"invalid_json"})";
    }
    catch (const std::runtime_error& e) {
        // Erreurs métier qu'on connaît (lancées par AuthServiceCore)
        std::string msg = e.what();
        http::status st = http::status::unauthorized;
        std::string code;

        if (msg == "invalid_credentials") {
            code = "invalid_credentials";
            st = http::status::unauthorized;
        } else if (msg == "user_disabled") {
            code = "user_disabled";
            st = http::status::forbidden;
        } else {
            // Pas un code métier prévu -> on log et on renvoie internal_error
            std::cerr << "[auth-service] login runtime_error: " << msg << "\n";
            code = "internal_error";
            st = http::status::internal_server_error;
        }

        res.result(st);
        res.body() = std::string("{\"error\":\"") + code + "\"}";
    }
    catch (const std::exception& e) {
        // Ici, on NE met PAS e.what() dans le JSON (risque d'UTF-8 pourri)
        std::cerr << "[auth-service] login std::exception: " << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }
    catch (...) {
        std::cerr << "[auth-service] login unknown exception\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}

// -----------------------------------------------------------------------------
// /auth/reset-password  (demande de reset : envoi du mail / log du lien)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleRequestPasswordReset(
    const http::request<http::string_body>& req) {

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.prepare_payload();
            return res;
        }

        auto body = json::parse(req.body());
        std::string email = body.at("email").get<std::string>();

        passwordReset_.requestReset(email);

        // Toujours OK (même si email inconnu) pour éviter de leak l'existence du compte
        res.body() = R"({"status":"ok"})";
        res.prepare_payload();
        return res;
    }
    catch (const json::exception&) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"invalid_json"})";
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] reset-password request exception: "
                  << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}

// -----------------------------------------------------------------------------
// /auth/reset-password/confirm  (consommer le token + changer le mot de passe)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleConfirmPasswordReset(
    const http::request<http::string_body>& req) {

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.prepare_payload();
            return res;
        }

        auto body = json::parse(req.body());
        std::string token       = body.at("token").get<std::string>();
        std::string newPassword = body.at("new_password").get<std::string>();

        if (newPassword.size() < 8) {
            res.result(http::status::bad_request);
            res.body() = R"({"error":"password_too_short"})";
            res.prepare_payload();
            return res;
        }

        passwordReset_.confirmReset(token, newPassword);

        res.body() = R"({"status":"ok"})";
        res.prepare_payload();
        return res;
    }
    catch (const json::exception&) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"invalid_json"})";
    }
    catch (const std::runtime_error& e) {
        std::string msg = e.what();
        std::string code;
        http::status st;

        if (msg == "invalid_token") {
            code = "invalid_token";
            st = http::status::bad_request;
        } else if (msg == "user_not_found") {
            code = "user_not_found";
            st = http::status::not_found;
        } else {
            std::cerr << "[auth-service] reset-password confirm runtime_error: "
                      << msg << "\n";
            code = "internal_error";
            st = http::status::internal_server_error;
        }

        res.result(st);
        res.body() = std::string("{\"error\":\"") + code + "\"}";
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] reset-password confirm exception: "
                  << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }
    catch (...) {
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}


// -----------------------------------------------------------------------------
// /auth/logout  (stateless : le client supprime simplement ses tokens)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleLogout(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.prepare_payload();
            return res;
        }

        res.body() = R"({"status":"logged_out"})";
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] logout exception: " << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}


// -----------------------------------------------------------------------------
// /auth/refresh  (renvoie un nouveau couple access/refresh token)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleRefresh(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.prepare_payload();
            return res;
        }

        // On attend un body JSON du type : { "refresh_token": "..." }
        auto body = json::parse(req.body());
        std::string refresh_token = body.at("refresh_token").get<std::string>();

        // Helpers pour relire la config JWT depuis les variables d'environnement
        auto env_str = [](const char* name, const char* def) {
            if (const char* v = std::getenv(name)) {
                return std::string(v);
            }
            return std::string(def);
        };
        auto env_seconds = [&](const char* name, const char* def) {
            std::string s = env_str(name, def);
            int value = 0;
            try {
                value = std::stoi(s);
            } catch (...) {
                value = std::stoi(def);
            }
            return std::chrono::seconds{value};
        };

        std::string secret      = env_str("JWT_SECRET",       "dev-secret-a-changer");
        std::string issuer      = env_str("JWT_ISSUER",       "securecloud-auth");
        auto        access_ttl  = env_seconds("JWT_ACCESS_TTL",  "900");     // 15 min
        auto        refresh_ttl = env_seconds("JWT_REFRESH_TTL", "604800");  // 7 jours


        // 1) Décoder le refresh_token
        auto decoded = jwt::decode(refresh_token);

        // 2) Vérifier signature / issuer / audience / expiry
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(issuer)
            .with_audience("securecloud-client");

        try {
            verifier.verify(decoded);
        } catch (const std::exception&) {
            throw std::runtime_error("invalid_refresh_token");
        }

        // 3) Vérifier que c'est bien un token de type "refresh"
        std::string typ;
        if (decoded.has_payload_claim("typ")) {
            typ = decoded.get_payload_claim("typ").as_string();
        }
        if (typ != "refresh") {
            throw std::runtime_error("invalid_refresh_token");
        }

        // 4) On récupère le subject (user id)
        if (!decoded.has_subject()) {
            throw std::runtime_error("invalid_refresh_token");
        }
        std::string sub = decoded.get_subject();

        using clock = std::chrono::system_clock;
        auto now         = clock::now();
        auto access_exp  = now + access_ttl;
        auto refresh_exp = now + refresh_ttl;

        // 5) Construire un nouveau access_token
        auto access_builder = jwt::create()
            .set_issuer(issuer)
            .set_audience("securecloud-client")
            .set_subject(sub)
            .set_issued_at(now)
            .set_expires_at(access_exp);

        // On propage quelques claims optionnels si le refresh_token les contient
        auto copy_claim_str = [&](const char* name) {
            if (decoded.has_payload_claim(name)) {
                std::string v = decoded.get_payload_claim(name).as_string();
                access_builder.set_payload_claim(name, jwt::claim(v));
            }
        };
        copy_claim_str("email");
        copy_claim_str("name");
        copy_claim_str("tenant");

        std::string new_access_token =
            access_builder.sign(jwt::algorithm::hs256{secret});

        // 6) Construire un nouveau refresh_token
        auto refresh_builder = jwt::create()
            .set_issuer(issuer)
            .set_audience("securecloud-client")
            .set_subject(sub)
            .set_issued_at(now)
            .set_expires_at(refresh_exp)
            .set_payload_claim("typ", jwt::claim(std::string("refresh")));

        // Propagation facultative de "tenant" si présent
        if (decoded.has_payload_claim("tenant")) {
            std::string v = decoded.get_payload_claim("tenant").as_string();
            refresh_builder.set_payload_claim("tenant", jwt::claim(v));
        }

        std::string new_refresh_token =
            refresh_builder.sign(jwt::algorithm::hs256{secret});

        auto to_unix = [](std::chrono::system_clock::time_point tp) {
            return std::chrono::system_clock::to_time_t(tp);
        };

        json out = {
            {"access_token",  new_access_token},
            {"refresh_token", new_refresh_token},
            {"access_exp",    to_unix(access_exp)},
            {"refresh_exp",   to_unix(refresh_exp)},
            {"token_type",    "Bearer"}
        };

        res.body() = out.dump();
        res.prepare_payload();
        return res;
    }
    catch (const json::exception&) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"invalid_json"})";
    }
    catch (const std::runtime_error& e) {
        std::string msg = e.what();
        std::string code;
        http::status st;

        if (msg == "invalid_refresh_token") {
            code = "invalid_refresh_token";
            st   = http::status::unauthorized;
        } else {
            std::cerr << "[auth-service] refresh runtime_error: " << msg << "\n";
            code = "internal_error";
            st   = http::status::internal_server_error;
        }

        res.result(st);
        res.body() = std::string("{\"error\":\"") + code + "\"}";
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] refresh exception: " << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}


// -----------------------------------------------------------------------------
// /auth/profile  (GET = lire profil, PUT = mise à jour)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleProfile(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());

    try {
        // 1) Vérifier Authorization: Bearer <access_token>
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            res.result(http::status::unauthorized);
            res.body() = R"({"error":"unauthorized"})";
            res.prepare_payload();
            return res;
        }

        std::string auth = std::string(auth_it->value());
        const std::string prefix = "Bearer ";
        if (auth.rfind(prefix, 0) != 0 || auth.size() <= prefix.size()) {
            res.result(http::status::unauthorized);
            res.body() = R"({"error":"invalid_token"})";
            res.prepare_payload();
            return res;
        }

        std::string access_token = auth.substr(prefix.size());

        // Helpers ENV
        auto env_str = [](const char* name, const char* def) {
            if (const char* v = std::getenv(name)) {
                return std::string(v);
            }
            return std::string(def);
        };

        std::string secret = env_str("JWT_SECRET", "dev-secret-a-changer");
        std::string issuer = env_str("JWT_ISSUER", "securecloud-auth");


        // 2) Vérifier le token
        auto decoded = jwt::decode(access_token);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(issuer)
            .with_audience("securecloud-client");

        try {
            verifier.verify(decoded);
        } catch (const std::exception&) {
            throw std::runtime_error("invalid_token");
        }

        if (!decoded.has_subject()) {
            throw std::runtime_error("invalid_token");
        }

        std::string user_id = decoded.get_subject();

        // 3) GET = lire profil
        if (req.method() == http::verb::get) {
            auto userOpt = profile_.getProfile(user_id);
            if (!userOpt) {
                throw std::runtime_error("user_not_found");
            }
            const User& u = *userOpt;

            nlohmann::json out = {
                {"id",            u.id},
                {"tenant",        u.tenant_id},
                {"email",         u.email},
                {"display_name",  u.display_name},
                {"mfa_required",  u.mfa_required},
                {"status",        u.status}
            };

            res.body() = out.dump();
            res.prepare_payload();
            return res;
        }

        // 4) PUT/PATCH = mise à jour
        if (req.method() == http::verb::put || req.method() == http::verb::patch) {
            auto userOpt = profile_.getProfile(user_id);
            if (!userOpt) {
                throw std::runtime_error("user_not_found");
            }

            User u = *userOpt;
            auto body = nlohmann::json::parse(req.body());

            bool changed = false;

            if (body.contains("email") && !body["email"].is_null()) {
                std::string new_email = body["email"].get<std::string>();
                if (!new_email.empty() && new_email != u.email) {
                    u.email = new_email;
                    changed = true;
                }
            }

            if (body.contains("display_name") && !body["display_name"].is_null()) {
                std::string new_name = body["display_name"].get<std::string>();
                if (!new_name.empty() && new_name != u.display_name) {
                    u.display_name = new_name;
                    changed = true;
                }
            }

            if (changed) {
                profile_.updateProfile(u.id, u.email, u.display_name);
            }

            nlohmann::json out = {
                {"id",            u.id},
                {"tenant",        u.tenant_id},
                {"email",         u.email},
                {"display_name",  u.display_name},
                {"mfa_required",  u.mfa_required},
                {"status",        u.status}
            };

            res.body() = out.dump();
            res.prepare_payload();
            return res;
        }

        // Méthode non autorisée
        res.result(http::status::method_not_allowed);
        res.body() = R"({"error":"method_not_allowed"})";
    }
    catch (const nlohmann::json::exception&) {
        res.result(http::status::bad_request);
        res.body() = R"({"error":"invalid_json"})";
    }
    catch (const std::runtime_error& e) {
        std::string msg = e.what();
        std::string code;
        http::status st;

        if (msg == "invalid_token") {
            code = "invalid_token";
            st   = http::status::unauthorized;
        } else if (msg == "user_not_found") {
            code = "user_not_found";
            st   = http::status::not_found;
        } else {
            std::cerr << "[auth-service] profile runtime_error: " << msg << "\n";
            code = "internal_error";
            st   = http::status::internal_server_error;
        }

        res.result(st);
        res.body() = std::string("{\"error\":\"") + code + "\"}";
    }
    catch (const std::exception& e) {
        std::cerr << "[auth-service] profile exception: " << e.what() << "\n";
        res.result(http::status::internal_server_error);
        res.body() = R"({"error":"internal_error"})";
    }

    res.prepare_payload();
    return res;
}

// -----------------------------------------------------------------------------
// /auth/mfa/verify  (alias de /auth/login, mais dédié MFA)
// -----------------------------------------------------------------------------
http::response<http::string_body>
AuthController::handleMfaVerify(const http::request<http::string_body>& req) {
    
    return handleLogin(req);
}
