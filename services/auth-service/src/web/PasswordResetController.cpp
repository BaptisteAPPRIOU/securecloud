#include "web/PasswordResetController.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PasswordResetController::PasswordResetController(PasswordResetService& service)
    : service_(service) {}

http::response<http::string_body>
PasswordResetController::handleRequest(const http::request<http::string_body>& req) {
    http::response<http::string_body> res;

    try {
        if (req.method() != http::verb::post) {
            res.result(http::status::method_not_allowed);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"method_not_allowed"})";
            return res;
        }

        json body;
        try {
            body = json::parse(req.body());
        } catch (const json::exception&) {
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"invalid_json"})";
            return res;
        }

        if (!body.contains("email") || !body["email"].is_string()) {
            res.result(http::status::bad_request);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"error":"missing_email"})";
            return res;
        }

        const std::string email = body["email"].get<std::string>();

        // Appel du service métier
        service_.requestReset(email);

        json out = {
            {"status", "ok"}
        };

        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = out.dump();
    } catch (const std::exception&) {
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error":"internal_error"})";
    }

    return res;
}
