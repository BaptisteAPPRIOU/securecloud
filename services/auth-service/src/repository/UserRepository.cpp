#include "repository/UserRepository.hpp"

UserRepository::UserRepository(std::string conninfo)
    : conninfo_(std::move(conninfo))
{
}

// -------------------- Helpers internes --------------------

static void set_search_path(pqxx::work& tx)
{
    tx.exec0("SET search_path TO auth,public");
}

// -------------------- Requêtes -----------------------------

std::optional<User> UserRepository::findByEmail(const std::string& email)
{
    pqxx::connection conn{conninfo_};
    pqxx::work tx{conn};

    set_search_path(tx);

    auto r = tx.exec_params(R"SQL(
        SELECT
            user_identifier,
            tenant_identifier,
            user_email,
            user_display_name,
            pass_hash,
            password_algo,
            mfa_required,
            user_status
        FROM auth.users
        WHERE user_email = $1
    )SQL", email);

    if (r.empty()) {
        return std::nullopt;
    }

    const auto& row = r.front();
    User u;
    u.id             = row["user_identifier"].c_str();
    u.tenant_id      = row["tenant_identifier"].c_str();
    u.email          = row["user_email"].c_str();
    u.display_name   = row["user_display_name"].c_str();
    u.pass_hash      = row["pass_hash"].c_str();
    u.password_algo  = row["password_algo"].c_str();
    u.mfa_required   = row["mfa_required"].as<bool>();
    u.status         = row["user_status"].c_str();

    return u;
}

std::optional<User> UserRepository::findById(const std::string& id)
{
    pqxx::connection conn{conninfo_};
    pqxx::work tx{conn};

    set_search_path(tx);

    auto r = tx.exec_params(R"SQL(
        SELECT
            user_identifier,
            tenant_identifier,
            user_email,
            user_display_name,
            pass_hash,
            password_algo,
            mfa_required,
            user_status
        FROM auth.users
        WHERE user_identifier = $1
    )SQL", id);

    if (r.empty()) {
        return std::nullopt;
    }

    const auto& row = r.front();
    User u;
    u.id             = row["user_identifier"].c_str();
    u.tenant_id      = row["tenant_identifier"].c_str();
    u.email          = row["user_email"].c_str();
    u.display_name   = row["user_display_name"].c_str();
    u.pass_hash      = row["pass_hash"].c_str();
    u.password_algo  = row["password_algo"].c_str();
    u.mfa_required   = row["mfa_required"].as<bool>();
    u.status         = row["user_status"].c_str();

    return u;
}

void UserRepository::updateLastLogin(const std::string& user_id)
{
    pqxx::connection conn{conninfo_};
    pqxx::work tx{conn};

    set_search_path(tx);

    tx.exec_params(R"SQL(
        UPDATE auth.users
        SET last_login_timestamp = now()
        WHERE user_identifier = $1
    )SQL", user_id);

    tx.commit();
}

void UserRepository::updatePassword(const std::string& user_id,
                                    const std::string& new_hash,
                                    const std::string& algo)
{
    pqxx::connection conn{conninfo_};
    pqxx::work tx{conn};

    set_search_path(tx);

    tx.exec_params(R"SQL(
        UPDATE auth.users
        SET pass_hash = $1,
            password_algo = $2
        WHERE user_identifier = $3
    )SQL", new_hash, algo, user_id);

    tx.commit();
}

void UserRepository::updateProfile(const std::string& user_id,
                                   const std::string& new_email,
                                   const std::string& new_display_name)
{
    pqxx::connection conn{conninfo_};
    pqxx::work tx{conn};

    set_search_path(tx);

    tx.exec_params(R"SQL(
        UPDATE auth.users
        SET user_email = $1,
            user_display_name = $2
        WHERE user_identifier = $3
    )SQL", new_email, new_display_name, user_id);

    tx.commit();
}