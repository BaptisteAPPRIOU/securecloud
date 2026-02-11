#include <gtest/gtest.h>

#include "httpClient.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

std::string env_or(const char *name, const char *fallback) {
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    return value;
}

uint16_t parse_port_or_default(const std::string &value, uint16_t fallback) {
    try {
        const auto parsed = std::stoul(value);
        if (parsed == 0 || parsed > 65535) {
            return fallback;
        }
        return static_cast<uint16_t>(parsed);
    } catch (...) {
        return fallback;
    }
}

} // namespace

TEST(AuthSessionE2E, LoginRefreshLogoutRejectsOldAccessToken) {
    const std::string gateway_host = env_or("SC_E2E_GATEWAY_HOST", "127.0.0.1");
    const uint16_t gateway_port = parse_port_or_default(env_or("SC_E2E_GATEWAY_PORT", "8443"), 8443);
    const std::string email = env_or("SC_E2E_EMAIL", "admin@demo.local");
    const std::string password = env_or("SC_E2E_PASSWORD", "admin1234");
    const std::string protected_path = env_or("SC_E2E_PROTECTED_PATH", "/api/me");

    net::io_context io_context;
    gateway::HttpClient client(io_context, gateway_host, gateway_port, std::chrono::milliseconds(8000));

    const auto health = client.get("/health");
    if (!health.success) {
        GTEST_SKIP() << "Gateway is unreachable at " << gateway_host << ":" << gateway_port
                     << " (" << health.error_message << ")";
    }
    if (health.status_code != 200) {
        GTEST_SKIP() << "Gateway health endpoint returned " << health.status_code
                     << ". Start gateway/auth before running this integration test.";
    }

    nlohmann::json login_payload = {
        {"email", email},
        {"password", password}
    };
    const auto login = client.post("/api/login", login_payload);
    ASSERT_TRUE(login.success) << "Login request failed: " << login.error_message;
    ASSERT_EQ(login.status_code, 200)
        << "Login failed. Configure SC_E2E_EMAIL/SC_E2E_PASSWORD if needed. Body: " << login.body;

    const auto login_json = nlohmann::json::parse(login.body, nullptr, false);
    ASSERT_TRUE(login_json.is_object()) << "Login response is not JSON: " << login.body;
    ASSERT_TRUE(login_json.contains("access_token"));
    ASSERT_TRUE(login_json.contains("refresh_token"));
    ASSERT_TRUE(login_json["access_token"].is_string());
    ASSERT_TRUE(login_json["refresh_token"].is_string());

    const std::string access_token = login_json["access_token"].get<std::string>();
    const std::string refresh_token = login_json["refresh_token"].get<std::string>();
    ASSERT_FALSE(access_token.empty());
    ASSERT_FALSE(refresh_token.empty());

    const auto pre_logout = client.get(
        protected_path,
        {{"Authorization", "Bearer " + access_token}}
    );
    ASSERT_TRUE(pre_logout.success) << "Protected call before logout failed: " << pre_logout.error_message;
    EXPECT_NE(pre_logout.status_code, 401)
        << "Access token rejected before logout. Body: " << pre_logout.body;

    const auto refresh = client.post("/api/refresh", nlohmann::json{{"refresh_token", refresh_token}});
    ASSERT_TRUE(refresh.success) << "Refresh request failed: " << refresh.error_message;
    ASSERT_EQ(refresh.status_code, 200) << "Refresh failed. Body: " << refresh.body;

    const auto logout = client.post(
        "/api/logout",
        nlohmann::json{{"refresh_token", refresh_token}},
        {{"Authorization", "Bearer " + access_token}}
    );
    ASSERT_TRUE(logout.success) << "Logout request failed: " << logout.error_message;
    ASSERT_EQ(logout.status_code, 200) << "Logout failed. Body: " << logout.body;

    const auto reused_old_access = client.get(
        protected_path,
        {{"Authorization", "Bearer " + access_token}}
    );
    ASSERT_TRUE(reused_old_access.success)
        << "Protected call with old token failed at transport level: " << reused_old_access.error_message;
    EXPECT_EQ(reused_old_access.status_code, 401)
        << "Old access token should be rejected after logout. Body: " << reused_old_access.body;
}
