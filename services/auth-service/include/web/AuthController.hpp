#pragma once
#include "service/AuthServiceCore.hpp"
#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

class AuthController {
public:
    explicit AuthController(AuthServiceCore& core);

    http::response<http::string_body>
    handleLogin(const http::request<http::string_body>& req);

private:
    AuthServiceCore& core_;
};
