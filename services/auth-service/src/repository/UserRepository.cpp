#include "repository/UserRepository.hpp"
#include <pqxx/pqxx>
#include <stdexcept>

UserRepository::UserRepository(std::string conninfo)
    : conninfo_(std::move(conninfo)) {}

std::optional<User> UserRepository::findByEmail(const std::string& email) {
    pqxx::connection c{conninfo_};
    pqxx::work tx{c};

    // on force le schéma auth
    auto res = tx.exec_params(R"SQL(
        SELECT user_identifier,
               tenant_identifier,
               user_email,
               user_display_name,
               pass_hash,
               password_algo,
               mfa_required,
               user_status,
               last_login_timestamp
        FROM auth.users
        WHERE user_email = $1
    )SQL", email);

    if (res.empty())
        return std::nullopt;

    const auto& row = res[0];
    User u;
    u.id           = row["user_identifier"].c_str();
    u.tenant_id    = row["tenant_identifier"].c_str();
    u.email        = row["user_email"].c_str();
    u.display_name = row["user_display_name"].c_str();
    u.pass_hash    = row["pass_hash"].c_str();
    u.pass_algo    = row["password_algo"].c_str();
    u.mfa_required = row["mfa_required"].as<bool>();
    u.status       = row["user_status"].c_str();

    if (!row["last_login_timestamp"].is_null()) {
        // on laisse optionnel, tu peux parser en time_point plus tard
        u.last_login = std::nullopt;
    }

    return u;
}

void UserRepository::updateLastLogin(const std::string& user_id) {
    pqxx::connection c{conninfo_};
    pqxx::work tx{c};

    tx.exec_params(R"SQL(
        UPDATE auth.users
        SET last_login_timestamp = now()
        WHERE user_identifier = $1
    )SQL", user_id);

    tx.commit();
}
