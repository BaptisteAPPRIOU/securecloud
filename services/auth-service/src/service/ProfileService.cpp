#include "service/ProfileService.hpp"
#include <stdexcept>

ProfileService::ProfileService(UserRepository& users)
    : users_(users) {}

std::optional<User> ProfileService::getProfile(const std::string& user_id) {
    return users_.findById(user_id);
}

void ProfileService::updateProfile(const std::string& user_id,
                                   const std::string& new_email,
                                   const std::string& new_display_name) {
    auto existing = users_.findById(user_id);
    if (!existing) {
        throw std::runtime_error("user_not_found");
    }

    users_.updateProfile(user_id, new_email, new_display_name);
}