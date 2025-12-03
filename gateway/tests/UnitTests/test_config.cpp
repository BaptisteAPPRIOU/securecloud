#include <gtest/gtest.h>
#include "gateway/config.hpp"
#include <fstream>

using namespace gateway;

TEST(ConfigTest, DefaultValuesWhenNoFile) {
    // Temporarily rename config file if exists
    std::ifstream f("config/gateway.dev.yaml");
    bool had = f.good();
    f.close();

    // If file exists, move it
    if (had) {
        std::rename("config/gateway.dev.yaml", "config/gateway.dev.yaml.bak");
    }

    ServerConfig cfg = [](){ return load_server_config(); }();
    EXPECT_EQ(cfg.host, "127.0.0.1");
    EXPECT_EQ(cfg.port, 8080);

    if (had) {
        std::rename("config/gateway.dev.yaml.bak", "config/gateway.dev.yaml");
    }
}

TEST(ConfigTest, ParsesHostAndPort) {
    // Use absolute or build-relative path to ensure file is created in correct location
    std::string test_path = "test_gateway_temp.yaml";
    std::ofstream out(test_path);
    out << "server:\n  host: 127.0.0.1\n  port: 9999\n";
    out.close();

    ServerConfig cfg = [&test_path](){ return load_server_config(test_path); }();
    EXPECT_EQ(cfg.host, "127.0.0.1");
    EXPECT_EQ(cfg.port, 9999);

    // cleanup
    std::remove(test_path.c_str());
}

