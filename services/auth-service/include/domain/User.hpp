#pragma once
#include <string>
#include <optional>
#include <chrono>

struct User {
    std::string id;            // user_identifier (uuid)
    std::string tenant_id;     // tenant_identifier (uuid)
    std::string email;         // user_email
    std::string display_name;  // user_display_name
    std::string pass_hash;     // pass_hash
    std::string pass_algo;     // password_algo
    bool        mfa_required;  // mfa_required
    std::string status;        // user_status
    std::optional<std::chrono::system_clock::time_point> last_login;
};
