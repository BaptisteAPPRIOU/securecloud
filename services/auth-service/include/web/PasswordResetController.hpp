#pragma once
#include "service/PasswordResetService.hpp"
#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

class PasswordResetController {
public:
    explicit PasswordResetController(PasswordResetService& service);

    // Gère POST /auth/reset-password (et alias /ForgetPassword)
    http::response<http::string_body>
    handleRequest(const http::request<http::string_body>& req);

private:
    PasswordResetService& service_;
};
