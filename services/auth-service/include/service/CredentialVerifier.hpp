#pragma once
#include "domain/User.hpp"
#include <string>

class CredentialVerifier {
public:
    bool verifyPassword(const User& user, const std::string& password) const;

private:
    static std::string sha256(const std::string& data);
};
