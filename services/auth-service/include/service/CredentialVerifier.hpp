#pragma once

#include <string>
#include "domain/User.hpp"

class CredentialVerifier {
public:
    bool verifyPassword(const User& user, const std::string& candidate) const;
};
