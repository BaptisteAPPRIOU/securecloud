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

    if (!body.contains("email") || !body.contains("password")) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"missing_fields"})";
        return res;
    }

    std::string email    = body["email"].get<std::string>();
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

        // Création des JWT
        using clock = std::chrono::system_clock;
        auto now = clock::now();
        auto access_exp  = now + access_ttl;
        auto refresh_exp = now + refresh_ttl;

        // Simplified approach - no custom claims
        auto access_token = jwt::create()
            .set_type("JWT")
            .set_issuer(jwt_issuer)
            .set_subject(user_id)  // Contains user_identifier
            .set_audience("securecloud-client")
            .set_issued_at(now)
            .set_expires_at(access_exp)
            .sign(jwt::algorithm::hs256{jwt_secret});

        auto refresh_token = jwt::create()
            .set_type("JWT")
            .set_issuer(jwt_issuer)
            .set_subject(user_id)  // Contains user_identifier
            .set_audience("securecloud-client")
            .set_issued_at(now)
            .set_expires_at(refresh_exp)
            .sign(jwt::algorithm::hs256{jwt_secret});

        // Return additional user info in HTTP response body instead of JWT
        auto to_epoch = [](const clock::time_point& tp) {
            return std::chrono::duration_cast<std::chrono::seconds>(
                tp.time_since_epoch()).count();
        };

        json out = {
            {"access_token",  access_token},
            {"refresh_token", refresh_token},
            {"token_type",    "Bearer"},
            {"access_exp",    to_epoch(access_exp)},
            {"refresh_exp",   to_epoch(refresh_exp)},
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

// ----- main : serveur HTTP sync sur /auth/login -----

int main() {
    // Connexion DB
    std::string conn =
        "host=" + env("DB_HOST","127.0.0.1") +
        " port=" + env("DB_PORT","15432") +
        " dbname=" + env("DB_NAME","securecloud_dev") +
        " user=" + env("DB_USER","securecloud") +
        " password=" + env("DB_PASS","securecloud");

    // JWT
    std::string jwt_issuer = env("JWT_ISSUER", "securecloud-auth");
    std::string jwt_secret = env("JWT_SECRET", "dev-secret-change-me");

    // env(...) retourne std::string → on passe .c_str() à atoi()
    std::chrono::seconds access_ttl{
        std::atoi(env("JWT_ACCESS_TTL","900").c_str())
    }; // 15 min

    std::chrono::seconds refresh_ttl{
        std::atoi(env("JWT_REFRESH_TTL","604800").c_str())
    }; // 7 jours

    // HTTP bind
    std::string bind_addr = env("AUTH_BIND_ADDR", "0.0.0.0");
    unsigned short port = static_cast<unsigned short>(
        std::atoi(env("AUTH_PORT","8081").c_str())
    );


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

            if (req.target() == "/auth/login") {
                res = handle_login(req, conn, jwt_issuer, jwt_secret,
                                   access_ttl, refresh_ttl);
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
