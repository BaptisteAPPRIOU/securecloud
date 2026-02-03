#pragma once

#include <boost/beast/http.hpp>

#include "service/AuthServiceCore.hpp"
#include "service/PasswordResetService.hpp"
#include "service/ProfileService.hpp"

class AuthController {
public:
    AuthController(AuthServiceCore& core,
                   PasswordResetService& passwordReset,
                   ProfileService& profile);

    boost::beast::http::response<boost::beast::http::string_body>
    handleLogin(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleRequestPasswordReset(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleConfirmPasswordReset(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleLogout(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleRefresh(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleProfile(const boost::beast::http::request<boost::beast::http::string_body>& req);

    boost::beast::http::response<boost::beast::http::string_body>
    handleMfaVerify(const boost::beast::http::request<boost::beast::http::string_body>& req);

private:
    AuthServiceCore&      core_;
    PasswordResetService& passwordReset_;
    ProfileService&       profile_;
};