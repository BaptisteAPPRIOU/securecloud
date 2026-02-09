#pragma once
#include <string>

struct User {
    std::string id;
    std::string tenant_id;
    std::string email;
    std::string display_name;
    std::string pass_hash;
    std::string password_algo;
    bool        mfa_required;
    std::string status;
};
