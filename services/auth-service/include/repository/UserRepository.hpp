#pragma once
#include "domain/User.hpp"
#include <optional>
#include <string>

class UserRepository {
public:
    explicit UserRepository(std::string conninfo);

    std::optional<User> findByEmail(const std::string& email);
    void updateLastLogin(const std::string& user_id);

private:
    std::string conninfo_;
};
