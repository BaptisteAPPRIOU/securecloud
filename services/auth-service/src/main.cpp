#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <pqxx/pqxx>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>
#include <optional>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// ----- helpers env -----

static std::string env(const char* k, const char* d) {
    const char* v = std::getenv(k);
    return v ? v : d;
}

// ----- SHA256 pour password_algo = "sha256" -----

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>

std::string sha256(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP sha256 failed");
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    return oss.str();
}

bool verify_password(const std::string& algo,
                     const std::string& hash,
                     const std::string& password) {
    if (algo == "plain") {
        return hash == password;
    } else if (algo == "sha256") {
        return hash == sha256(password);
    }
    // algo non supporté
    return false;
}

std::string extract_bearer_token(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
        return {};
    }

    const auto auth_value = it->value();
    const std::string auth_header(auth_value.data(), auth_value.size());
    const std::string prefix = "Bearer ";
    if (auth_header.size() <= prefix.size() || auth_header.substr(0, prefix.size()) != prefix) {
        return {};
    }
    return auth_header.substr(prefix.size());
}

std::optional<std::string> extract_refresh_token_from_body(const http::request<http::string_body>& req) {
    if (req.body().empty()) {
        return std::nullopt;
    }

    auto body = json::parse(req.body(), nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        return std::nullopt;
    }

    if (!body.contains("refresh_token") || !body["refresh_token"].is_string()) {
        return std::nullopt;
    }

    const std::string refresh = body["refresh_token"].get<std::string>();
    if (refresh.empty()) {
        return std::nullopt;
    }
    return refresh;
}

std::string random_hex(std::size_t bytes_len) {
    std::string bytes(bytes_len, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&bytes[0]), static_cast<int>(bytes_len)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }

    std::ostringstream oss;
    for (unsigned char c : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

struct IssuedToken {
    std::string token;
    std::chrono::system_clock::time_point exp;
};

IssuedToken issue_token(const std::string& user_id,
                        const std::string& jwt_issuer,
                        const std::string& jwt_secret,
                        std::chrono::seconds ttl,
                        const std::string& token_type) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto exp = now + ttl;
    const std::string jti = random_hex(16);

    auto token = jwt::create()
        .set_type("JWT")
        .set_issuer(jwt_issuer)
        .set_subject(user_id)
        .set_audience("securecloud-client")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("typ", jwt::claim(token_type))
        .set_payload_claim("jti", jwt::claim(jti))
        .sign(jwt::algorithm::hs256{jwt_secret});

    return IssuedToken{std::move(token), exp};
}

long long to_epoch_seconds(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

template <typename DecodedJwt>
std::string revocation_hash_for_token(const DecodedJwt& decoded, const std::string& raw_token) {
    if (decoded.has_payload_claim("jti")) {
        try {
            const auto jti = decoded.get_payload_claim("jti").as_string();
            if (!jti.empty()) {
                return sha256(jti);
            }
        } catch (...) {
        }
    }
    return sha256(raw_token);
}

template <typename DecodedJwt>
std::optional<std::string> token_type_of(const DecodedJwt& decoded) {
    if (!decoded.has_payload_claim("typ")) {
        return std::nullopt;
    }
    try {
        return decoded.get_payload_claim("typ").as_string();
    } catch (...) {
        return std::nullopt;
    }
}

template <typename Tx, typename DecodedJwt>
void persist_revocation(Tx& tx, const DecodedJwt& decoded, const std::string& raw_token) {
    const std::string token_hash = revocation_hash_for_token(decoded, raw_token);
    if (decoded.has_expires_at()) {
        const auto exp_epoch = to_epoch_seconds(decoded.get_expires_at());
        tx.exec_params(
            "INSERT INTO revoked_jti (jti_hash, exp) "
            "VALUES ($1, to_timestamp($2)) "
            "ON CONFLICT (jti_hash) DO NOTHING",
            token_hash,
            exp_epoch
        );
    } else {
        tx.exec_params(
            "INSERT INTO revoked_jti (jti_hash) "
            "VALUES ($1) "
            "ON CONFLICT (jti_hash) DO NOTHING",
            token_hash
        );
    }
}

// ----- handler /auth/login -----

http::response<http::string_body>
handle_login(const http::request<http::string_body>& req,
             const std::string& db_conninfo,
             const std::string& jwt_issuer,
             const std::string& jwt_secret,
             std::chrono::seconds access_ttl,
             std::chrono::seconds refresh_ttl) {
    http::response<http::string_body> res;

    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"method_not_allowed"})";
        return res;
    }

    json body;
    try {
        body = json::parse(req.body());
    } catch (...) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_json"})";
        return res;
    }

    if ((!body.contains("email") && !body.contains("username")) || !body.contains("password")) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_fields"})";
        return res;
    }

    std::string email;
    if (body.contains("email")) {
        email = body["email"].get<std::string>();
    } else {
        email = body["username"].get<std::string>();
    }
    std::string password = body["password"].get<std::string>();

    try {
        // Connexion DB
        pqxx::connection c{db_conninfo};
        pqxx::work tx{c};

        tx.exec0("SET search_path TO auth,public");

        auto r = tx.exec_params(R"SQL(
            SELECT user_identifier,
                   tenant_identifier,
                   user_email,
                   user_display_name,
                   pass_hash,
                   password_algo,
                   mfa_required,
                   user_status
            FROM users
            WHERE user_email = $1
        )SQL", email);

        if (r.empty()) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_credentials"})";
            return res;
        }

        const auto& row = r[0];
        std::string user_id        = row["user_identifier"].c_str();
        std::string tenant_id      = row["tenant_identifier"].c_str();
        std::string user_email     = row["user_email"].c_str();
        std::string display_name   = row["user_display_name"].c_str();
        std::string pass_hash      = row["pass_hash"].c_str();
        std::string password_algo  = row["password_algo"].c_str();
        bool mfa_required          = row["mfa_required"].as<bool>();
        std::string status         = row["user_status"].c_str();

        if (status != "enabled") {
            res.result(http::status::forbidden);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"user_disabled"})";
            return res;
        }

        if (!verify_password(password_algo, pass_hash, password)) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_credentials"})";
            return res;
        }

        // last_login_timestamp
        tx.exec_params(R"SQL(
            UPDATE users
            SET last_login_timestamp = now()
            WHERE user_identifier = $1
        )SQL", user_id);
        tx.commit();

        // Création des JWT (typed + jti for better session handling)
        auto access  = issue_token(user_id, jwt_issuer, jwt_secret, access_ttl, "access");
        auto refresh = issue_token(user_id, jwt_issuer, jwt_secret, refresh_ttl, "refresh");

        json out = {
            {"access_token",  access.token},
            {"refresh_token", refresh.token},
            {"token_type",    "Bearer"},
            {"access_exp",    to_epoch_seconds(access.exp)},
            {"refresh_exp",   to_epoch_seconds(refresh.exp)},
            {"mfa_required",  mfa_required},
            {"user_id",       user_id},
            {"email",         user_email},
            {"tenant",        tenant_id},
            {"name",          display_name}  // Return in response, not in JWT
        };

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();

    } catch (const std::exception& e) {
        std::cerr << "login error: " << e.what() << std::endl;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
    }

    return res;
}

// ----- handler /auth/logout -----

http::response<http::string_body>
handle_refresh(const http::request<http::string_body>& req,
               const std::string& db_conninfo,
               const std::string& jwt_issuer,
               const std::string& jwt_secret,
               std::chrono::seconds access_ttl) {
    http::response<http::string_body> res;

    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"method_not_allowed"})";
        return res;
    }

    json body;
    try {
        body = json::parse(req.body());
    } catch (...) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_json"})";
        return res;
    }

    if (!body.contains("refresh_token") || !body["refresh_token"].is_string()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_fields"})";
        return res;
    }

    const std::string refresh_token = body["refresh_token"].get<std::string>();
    if (refresh_token.empty()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_fields"})";
        return res;
    }

    try {
        auto decoded = jwt::decode(refresh_token);
        auto verifier = jwt::verify()
            .with_issuer(jwt_issuer)
            .with_audience("securecloud-client")
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret});
        verifier.verify(decoded);

        const auto token_type = token_type_of(decoded);
        if (!token_type.has_value() || *token_type != "refresh") {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token_type"})";
            return res;
        }

        if (!decoded.has_subject() || decoded.get_subject().empty()) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token"})";
            return res;
        }
        const std::string user_id = decoded.get_subject();

        pqxx::connection c{db_conninfo};
        pqxx::work tx{c};
        tx.exec0("SET search_path TO auth,public");

        auto user_row = tx.exec_params(
            "SELECT user_status FROM users WHERE user_identifier = $1 LIMIT 1",
            user_id
        );
        const std::string user_status = user_row.empty()
            ? std::string()
            : std::string(user_row[0]["user_status"].c_str());
        if (user_status != "enabled") {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token"})";
            return res;
        }

        const std::string token_hash = revocation_hash_for_token(decoded, refresh_token);
        auto revoked = tx.exec_params(
            "SELECT 1 FROM revoked_jti WHERE jti_hash = $1 LIMIT 1",
            token_hash
        );
        tx.commit();

        if (!revoked.empty()) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"token_revoked"})";
            return res;
        }

        auto access = issue_token(user_id, jwt_issuer, jwt_secret, access_ttl, "access");

        json out = {
            {"access_token", access.token},
            {"token_type", "Bearer"},
            {"expires_in", access_ttl.count()},
            {"access_exp", to_epoch_seconds(access.exp)}
        };

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();
        return res;
    } catch (const jwt::error::token_verification_exception&) {
        res.result(http::status::unauthorized);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_token"})";
        return res;
    } catch (const std::exception& e) {
        std::cerr << "refresh error: " << e.what() << std::endl;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
        return res;
    }
}

http::response<http::string_body>
handle_validate(const http::request<http::string_body>& req,
                const std::string& db_conninfo,
                const std::string& jwt_issuer,
                const std::string& jwt_secret) {
    http::response<http::string_body> res;

    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"method_not_allowed"})";
        return res;
    }

    std::string token;
    // For /auth/validate, body field name is "token"; keep Bearer fallback for diagnostics.
    auto parsed = json::parse(req.body(), nullptr, false);
    if (!parsed.is_discarded() && parsed.is_object() &&
        parsed.contains("token") && parsed["token"].is_string()) {
        token = parsed["token"].get<std::string>();
    } else {
        token = extract_bearer_token(req);
    }

    if (token.empty()) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_token"})";
        return res;
    }

    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .with_issuer(jwt_issuer)
            .with_audience("securecloud-client")
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret});
        verifier.verify(decoded);

        const auto token_type = token_type_of(decoded);
        if (token_type.has_value() && *token_type != "access") {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token_type"})";
            return res;
        }

        if (!decoded.has_subject() || decoded.get_subject().empty()) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token"})";
            return res;
        }
        const std::string user_id = decoded.get_subject();

        pqxx::connection c{db_conninfo};
        pqxx::work tx{c};
        tx.exec0("SET search_path TO auth,public");

        auto user_row = tx.exec_params(
            "SELECT user_status FROM users WHERE user_identifier = $1 LIMIT 1",
            user_id
        );
        const std::string user_status = user_row.empty()
            ? std::string()
            : std::string(user_row[0]["user_status"].c_str());
        if (user_status != "enabled") {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token"})";
            return res;
        }

        const std::string token_hash = revocation_hash_for_token(decoded, token);
        auto revoked = tx.exec_params(
            "SELECT 1 FROM revoked_jti WHERE jti_hash = $1 LIMIT 1",
            token_hash
        );
        tx.commit();

        if (!revoked.empty()) {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"token_revoked"})";
            return res;
        }

        json out = {
            {"active", true},
            {"sub", user_id}
        };
        if (decoded.has_expires_at()) {
            out["exp"] = to_epoch_seconds(decoded.get_expires_at());
        }
        if (decoded.has_payload_claim("email")) {
            out["email"] = decoded.get_payload_claim("email").as_string();
        }
        if (decoded.has_payload_claim("tenant")) {
            out["tenant"] = decoded.get_payload_claim("tenant").as_string();
        }

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();
        return res;
    } catch (const jwt::error::token_verification_exception&) {
        res.result(http::status::unauthorized);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_token"})";
        return res;
    } catch (const std::exception& e) {
        std::cerr << "validate error: " << e.what() << std::endl;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
        return res;
    }
}

// ----- handler /auth/logout -----

http::response<http::string_body>
handle_logout(const http::request<http::string_body>& req,
              const std::string& db_conninfo,
              const std::string& jwt_issuer,
              const std::string& jwt_secret) {
    http::response<http::string_body> res;

    if (req.method() != http::verb::post) {
        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"method_not_allowed"})";
        return res;
    }

    const std::string access_token = extract_bearer_token(req);
    if (access_token.empty()) {
        res.result(http::status::unauthorized);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_authorization"})";
        return res;
    }

    const auto refresh_token = extract_refresh_token_from_body(req);

    try {
        auto access_decoded = jwt::decode(access_token);
        auto verifier = jwt::verify()
            .with_issuer(jwt_issuer)
            .with_audience("securecloud-client")
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret});
        verifier.verify(access_decoded);

        const auto access_type = token_type_of(access_decoded);
        if (access_type.has_value() && *access_type != "access") {
            res.result(http::status::unauthorized);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_token_type"})";
            return res;
        }

        pqxx::connection c{db_conninfo};
        pqxx::work tx{c};
        tx.exec0("SET search_path TO auth,public");

        persist_revocation(tx, access_decoded, access_token);

        if (refresh_token.has_value()) {
            auto refresh_decoded = jwt::decode(*refresh_token);
            verifier.verify(refresh_decoded);
            const auto refresh_type = token_type_of(refresh_decoded);
            if (!refresh_type.has_value() || *refresh_type != "refresh") {
                res.result(http::status::unauthorized);
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error":"invalid_token_type"})";
                return res;
            }
            persist_revocation(tx, refresh_decoded, *refresh_token);
        }

        tx.commit();

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"success":true})";
        return res;
    } catch (const jwt::error::token_verification_exception&) {
        res.result(http::status::unauthorized);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_token"})";
        return res;
    } catch (const std::exception& e) {
        std::cerr << "logout error: " << e.what() << std::endl;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
        return res;
    }
}

// ----- main : serveur HTTP sync -----

int main() {
    // Connexion DB
    std::string db_host = env("DB_HOST","127.0.0.1");
    std::string db_port = env("DB_PORT","15432");
    std::string db_name = env("DB_NAME","securecloud_dev");
    std::string db_user = env("DB_USER","securecloud");
    std::string db_pass = env("DB_PASS","securecloud");
    std::string conn =
        "host=" + db_host +
        " port=" + db_port +
        " dbname=" + db_name +
        " user=" + db_user +
        " password=" + db_pass;

    // JWT
    std::string jwt_issuer = env("JWT_ISSUER", "securecloud-auth");
    std::string jwt_secret = env("JWT_SECRET", "dev-secret-change-me");

    // env(...) retourne std::string → on passe .c_str() à atoi()
    std::string access_ttl_str = env("JWT_ACCESS_TTL","900");
    std::string refresh_ttl_str = env("JWT_REFRESH_TTL","604800");
    std::chrono::seconds access_ttl{
        std::atoi(access_ttl_str.c_str())
    }; // 15 min

    std::chrono::seconds refresh_ttl{
        std::atoi(refresh_ttl_str.c_str())
    }; // 7 jours

    // HTTP bind
    std::string bind_addr = env("AUTH_BIND_ADDR", "0.0.0.0");
    std::string port_str = env("AUTH_PORT", "");
    if (port_str.empty()) {
        port_str = env("SERVICE_PORT", "8081");
    }
    unsigned short port = static_cast<unsigned short>(
        std::atoi(port_str.c_str())
    );

    auto mask_value = [](const std::string& value) {
        return value.empty() ? std::string("<empty>") : std::string("<set>");
    };

    std::cout << "=== Auth Service Configuration ===" << std::endl;
    std::cout << "DB_HOST=" << db_host << std::endl;
    std::cout << "DB_PORT=" << db_port << std::endl;
    std::cout << "DB_NAME=" << db_name << std::endl;
    std::cout << "DB_USER=" << db_user << std::endl;
    std::cout << "DB_PASS=" << mask_value(db_pass) << std::endl;
    std::cout << "JWT_ISSUER=" << jwt_issuer << std::endl;
    std::cout << "JWT_SECRET=" << mask_value(jwt_secret) << std::endl;
    std::cout << "JWT_ACCESS_TTL=" << access_ttl_str << std::endl;
    std::cout << "JWT_REFRESH_TTL=" << refresh_ttl_str << std::endl;
    std::cout << "AUTH_BIND_ADDR=" << bind_addr << std::endl;
    std::cout << "AUTH_PORT=" << port_str << std::endl;
    std::cout << "==================================" << std::endl;


    try {
        net::io_context ioc{1};
        tcp::endpoint endpoint{ net::ip::make_address(bind_addr), port };
        tcp::acceptor acceptor{ioc, endpoint};

        std::cout << "auth-service listening on " << bind_addr << ":" << port << std::endl;

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            beast::error_code ec;

            http::read(socket, buffer, req, ec);
            if (ec) {
                std::cerr << "read error: " << ec.message() << std::endl;
                continue;
            }

            http::response<http::string_body> res;

            if (req.target() == "/health") {
                res.result(http::status::ok);
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"status":"ok"})";
            } else if (req.target() == "/auth/login") {
                res = handle_login(req, conn, jwt_issuer, jwt_secret,
                                   access_ttl, refresh_ttl);
            } else if (req.target() == "/auth/refresh") {
                res = handle_refresh(req, conn, jwt_issuer, jwt_secret, access_ttl);
            } else if (req.target() == "/auth/validate") {
                res = handle_validate(req, conn, jwt_issuer, jwt_secret);
            } else if (req.target() == "/auth/logout") {
                res = handle_logout(req, conn, jwt_issuer, jwt_secret);
            } else {
                res.result(http::status::not_found);
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error":"not_found"})";
            }

            res.version(req.version());
            res.keep_alive(false);
            res.content_length(res.body().size());

            http::write(socket, res, ec);
            if (ec) {
                std::cerr << "write error: " << ec.message() << std::endl;
            }

            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    } catch (const std::exception& e) {
        std::cerr << "fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
