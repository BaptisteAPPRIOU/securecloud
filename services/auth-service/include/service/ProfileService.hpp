#pragma once

#include "repository/UserRepository.hpp"
#include <optional>
#include <string>

class ProfileService {
public:
    explicit ProfileService(UserRepository& users);

    std::optional<User> getProfile(const std::string& user_id);

    void updateProfile(const std::string& user_id,
                       const std::string& new_email,
                       const std::string& new_display_name);

private:
    UserRepository& users_;
};
