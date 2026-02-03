#pragma once

#include <optional>
#include <string>
#include <pqxx/pqxx>

#include "domain/User.hpp"

class UserRepository {
public:
    explicit UserRepository(std::string conninfo);

    std::optional<User> findByEmail(const std::string& email);
    std::optional<User> findById(const std::string& id);

    void updateLastLogin(const std::string& user_id);
    void updatePassword(const std::string& user_id,
                        const std::string& new_hash,
                        const std::string& algo);
    void updateProfile(const std::string& user_id,
                       const std::string& new_email,
                       const std::string& new_display_name);

private:
    std::string conninfo_;
};
