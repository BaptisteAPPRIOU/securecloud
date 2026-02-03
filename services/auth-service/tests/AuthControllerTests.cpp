#include <gtest/gtest.h>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>
#include <cstdlib>

#include "web/AuthController.hpp"
#include "service/AuthServiceCore.hpp"
#include "service/PasswordResetService.hpp"
#include "service/ProfileService.hpp"
#include "service/CredentialVerifier.hpp"
#include "service/JwtService.hpp"
#include "repository/UserRepository.hpp"

namespace http = boost::beast::http;

// Petit helper pour définir des variables d'environnement de façon portable
static void set_env(const char* name, const char* value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

// Fixture commune pour tous les tests d'AuthController
class AuthControllerFixture : public ::testing::Test {
protected:
    AuthControllerFixture()
        : userRepo{"host=127.0.0.1 port=15432 dbname=securecloud_dev user=securecloud password=securecloud"},
          jwtService{"securecloud-auth", "test-secret",
                     std::chrono::seconds{900},     // access TTL
                     std::chrono::seconds{604800}}, // refresh TTL
          credVerifier{},
          resetCfg{
              .issuer         = "securecloud-auth",
              .secret         = "test-secret",
              .token_ttl      = std::chrono::seconds{3600},
              .reset_url_base = "http://localhost/reset-password",
              .smtp_host      = "",        // vide => pas de vrai SMTP
              .smtp_port      = 0,
              .smtp_from      = "no-reply@test.local"
          },
          authCore{userRepo, credVerifier, jwtService},
          passwordReset{userRepo, resetCfg},
          profileService{userRepo},
          controller{authCore, passwordReset, profileService}
    {
        // On aligne les variables d'env sur la config utilisée par JwtService
        set_env("JWT_SECRET",       "test-secret");
        set_env("JWT_ISSUER",       "securecloud-auth");
        set_env("JWT_ACCESS_TTL",   "900");
        set_env("JWT_REFRESH_TTL",  "604800");
    }

    UserRepository      userRepo;
    JwtService          jwtService;
    CredentialVerifier  credVerifier;
    PasswordResetConfig resetCfg;
    AuthServiceCore     authCore;
    PasswordResetService passwordReset;
    ProfileService      profileService;
    AuthController      controller;
};

//
// Tests /auth/login
//

TEST_F(AuthControllerFixture, LoginRejectsGetMethod)
{
    http::request<http::string_body> req{http::verb::get, "/auth/login", 11};
    req.keep_alive(false);

    auto res = controller.handleLogin(req);

    EXPECT_EQ(res.result(), http::status::method_not_allowed);
    EXPECT_EQ(res[http::field::content_type], "application/json");
    EXPECT_EQ(res.body(), R"({"error":"method_not_allowed"})");
}

TEST_F(AuthControllerFixture, LoginInvalidJsonReturnsBadRequest)
{
    http::request<http::string_body> req{http::verb::post, "/auth/login", 11};
    req.set(http::field::content_type, "application/json");
    req.body() = "{ invalid json";
    req.prepare_payload();

    auto res = controller.handleLogin(req);

    EXPECT_EQ(res.result(), http::status::bad_request);
    EXPECT_EQ(res.body(), R"({"error":"invalid_json"})");
}

TEST_F(AuthControllerFixture, LoginMissingPasswordReturnsBadRequest)
{
    http::request<http::string_body> req{http::verb::post, "/auth/login", 11};
    req.set(http::field::content_type, "application/json");

    // Pas de champ "password" => json::exception dans handleLogin
    nlohmann::json body = {
        {"email", "admin@demo.local"}
    };
    req.body() = body.dump();
    req.prepare_payload();

    auto res = controller.handleLogin(req);

    EXPECT_EQ(res.result(), http::status::bad_request);
    EXPECT_EQ(res.body(), R"({"error":"invalid_json"})");
}

//
// Tests /auth/logout
//

TEST_F(AuthControllerFixture, LogoutRejectsGetMethod)
{
    http::request<http::string_body> req{http::verb::get, "/auth/logout", 11};
    req.keep_alive(false);

    auto res = controller.handleLogout(req);

    EXPECT_EQ(res.result(), http::status::method_not_allowed);
}

TEST_F(AuthControllerFixture, LogoutPostReturnsLoggedOut)
{
    http::request<http::string_body> req{http::verb::post, "/auth/logout", 11};
    req.keep_alive(false);

    auto res = controller.handleLogout(req);

    EXPECT_EQ(res.result(), http::status::ok);
    EXPECT_EQ(res.body(), R"({"status":"logged_out"})");
}

//
// Tests /auth/refresh
//

TEST_F(AuthControllerFixture, RefreshInvalidJsonReturnsBadRequest)
{
    http::request<http::string_body> req{http::verb::post, "/auth/refresh", 11};
    req.set(http::field::content_type, "application/json");
    req.body() = "{ invalid json";
    req.prepare_payload();

    auto res = controller.handleRefresh(req);

    EXPECT_EQ(res.result(), http::status::bad_request);
    EXPECT_EQ(res.body(), R"({"error":"invalid_json"})");
}

// refresh_token syntactiquement valide mais signé avec un mauvais secret
TEST_F(AuthControllerFixture, RefreshWithInvalidTokenReturnsUnauthorized)
{
    using clock = std::chrono::system_clock;

    auto invalid_refresh = jwt::create()
        .set_issuer("wrong-issuer")
        .set_subject("user-123")
        .set_audience("securecloud-client")
        .set_issued_at(clock::now())
        .set_expires_at(clock::now() + std::chrono::seconds{60})
        .set_payload_claim("typ", jwt::claim(std::string("refresh")))
        .sign(jwt::algorithm::hs256{"wrong-secret"});

    http::request<http::string_body> req{http::verb::post, "/auth/refresh", 11};
    req.set(http::field::content_type, "application/json");

    nlohmann::json body = {
        {"refresh_token", invalid_refresh}
    };
    req.body() = body.dump();
    req.prepare_payload();

    auto res = controller.handleRefresh(req);

    EXPECT_EQ(res.result(), http::status::unauthorized);
    EXPECT_EQ(res.body(), R"({"error":"invalid_refresh_token"})");
}

// refresh_token valide généré via JwtService
TEST_F(AuthControllerFixture, RefreshWithValidTokenReturnsNewTokens)
{
    using clock = std::chrono::system_clock;

    // On fabrique un user "fake" utilisé uniquement pour signer le JWT
    User u;
    u.id          = "user-123";
    u.tenant_id   = "tenant-1";
    u.email       = "admin@demo.local";
    u.display_name = "Admin";
    u.pass_hash   = "";
    u.mfa_required = false;
    u.status      = "enabled";

    TokenPair pair = jwtService.issueTokens(u);

    http::request<http::string_body> req{http::verb::post, "/auth/refresh", 11};
    req.set(http::field::content_type, "application/json");

    nlohmann::json body = {
        {"refresh_token", pair.refresh_token}
    };
    req.body() = body.dump();
    req.prepare_payload();

    auto res = controller.handleRefresh(req);

    EXPECT_EQ(res.result(), http::status::ok);
    EXPECT_EQ(res[http::field::content_type], "application/json");

    auto out = nlohmann::json::parse(res.body());
    EXPECT_TRUE(out.contains("access_token"));
    EXPECT_TRUE(out.contains("refresh_token"));
    EXPECT_TRUE(out.contains("access_exp"));
    EXPECT_TRUE(out.contains("refresh_exp"));
    EXPECT_EQ(out.value("token_type", ""), "Bearer");
}

//
// Tests /auth/profile
//

TEST_F(AuthControllerFixture, ProfileWithoutAuthorizationHeaderIsUnauthorized)
{
    http::request<http::string_body> req{http::verb::get, "/auth/profile", 11};
    req.keep_alive(false);

    auto res = controller.handleProfile(req);

    EXPECT_EQ(res.result(), http::status::unauthorized);
    EXPECT_EQ(res.body(), R"({"error":"unauthorized"})");
}

// Authorization: Bearer <JWT> mais signature invalide
TEST_F(AuthControllerFixture, ProfileWithInvalidTokenReturnsUnauthorized)
{
    using clock = std::chrono::system_clock;

    auto invalid_access = jwt::create()
        .set_issuer("wrong-issuer")
        .set_subject("user-123")
        .set_audience("securecloud-client")
        .set_issued_at(clock::now())
        .set_expires_at(clock::now() + std::chrono::seconds{60})
        .sign(jwt::algorithm::hs256{"wrong-secret"});

    http::request<http::string_body> req{http::verb::get, "/auth/profile", 11};
    req.set(http::field::authorization,
            "Bearer " + invalid_access);
    req.keep_alive(false);

    auto res = controller.handleProfile(req);

    EXPECT_EQ(res.result(), http::status::unauthorized);
    EXPECT_EQ(res.body(), R"({"error":"invalid_token"})");
}
