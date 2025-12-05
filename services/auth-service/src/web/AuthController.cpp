#include "web/AuthController.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AuthController::AuthController(AuthServiceCore& core)
    : core_(core) {}

http::response<http::string_body>
AuthController::handleLogin(const http::request<http::string_body>& req) {
    http::response<http::string_body> res;

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.body() = R"({"error":"method_not_allowed"})";
            res.set(http::field::content_type, "application/json");
            return res;
        }

        auto j = json::parse(req.body(), nullptr, true);

        LoginRequest lr;
        lr.email    = j.at("email").get<std::string>();
        lr.password = j.at("password").get<std::string>();
        if (j.contains("mfa") && !j["mfa"].is_null())
            lr.mfa = j["mfa"].get<std::string>();

        auto result = core_.login(lr);

        auto access_exp  = std::chrono::duration_cast<std::chrono::seconds>(
            result.tokens.access_exp.time_since_epoch()).count();
        auto refresh_exp = std::chrono::duration_cast<std::chrono::seconds>(
            result.tokens.refresh_exp.time_since_epoch()).count();

        json out = {
            {"access_token",  result.tokens.access_token},
            {"refresh_token", result.tokens.refresh_token},
            {"token_type",    "Bearer"},
            {"access_exp",    access_exp},
            {"refresh_exp",   refresh_exp},
            {"mfa_required",  result.mfa_required}
        };

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();
    } catch (const nlohmann::json::exception&) {
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"invalid_json"})";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        http::status st = http::status::unauthorized;
        if (msg == "user_disabled")
            st = http::status::forbidden;

        json out = { {"error", msg} };
        res.result(st);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();
    } catch (...) {
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
    }

    return res;
}
