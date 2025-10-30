#include <pqxx/pqxx>
#include <cstdlib>
#include <iostream>

static std::string env(const char* k, const char* d) {
  const char* v = std::getenv(k);
  return v ? v : d;
}

int main() {
  std::string conn =
    "host=" + env("DB_HOST","127.0.0.1") +
    " port=" + env("DB_PORT","5432") +
    " dbname=" + env("DB_NAME","securecloud_dev") +
    " user=" + env("DB_USER","securecloud") +
    " password=" + env("DB_PASS","securecloud");

  try {
    pqxx::connection c{conn};
    pqxx::work tx{c};

    // force le schéma de ce service
    tx.exec0("SET search_path TO auth,public");

    auto r = tx.exec("SELECT count(*) FROM users");
    std::cout << "users.count=" << r[0][0].as<long long>() << "\n";
    tx.commit();
  } catch (const std::exception& e) {
    std::cerr << "DB error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
