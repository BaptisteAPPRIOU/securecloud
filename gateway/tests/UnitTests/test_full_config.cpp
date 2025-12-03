#include <gtest/gtest.h>
#include "gateway/config.hpp"
#include <fstream>
#include <algorithm>

using namespace gateway;

class FullConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_config_path = "test_full_gateway.yaml";
    }

    void TearDown() override {
        std::remove(test_config_path.c_str());
    }

    void create_full_config() {
        std::ofstream out(test_config_path);
        out << R"(server:
  host: 0.0.0.0
  port: 8443
  tls:
    cert_file: config/certs/test-cert.pem
    key_file: config/certs/test-key.pem
    client_mtls: true

routing:
  - match: "^/v1/auth/.*"
    target: auth
  - match: "^/v1/rt/connect$"
    target: messaging
    upgrade: websocket
  - match: ".*"
    target: files

upstreams:
  auth:
    kind: http
    transport: uds
    socket: "/run/securecloud/auth.sock"
  messaging:
    kind: ws
    transport: tcp
    address: "messaging:8081"
  files:
    kind: http
    transport: uds
    socket: "/run/securecloud/files.sock"

security:
  jwks:
    cache_ttl_s: 600

observability:
  prometheus:
    bind: "0.0.0.0:9090"
  logs:
    level: debug
)";
        out.close();
    }

    std::string test_config_path;
};

TEST_F(FullConfigTest, ParsesServerConfig) {
    create_full_config();
    
    GatewayConfig cfg = load_gateway_config(test_config_path);
    
    EXPECT_EQ(cfg.server.host, "0.0.0.0");
    EXPECT_EQ(cfg.server.port, 8443);
}

TEST_F(FullConfigTest, ParsesRoutingRules) {
    create_full_config();
    
    GatewayConfig cfg = load_gateway_config(test_config_path);
    
    ASSERT_EQ(cfg.routes.size(), 3);
    
    EXPECT_EQ(cfg.routes[0].match_pattern, "^/v1/auth/.*");
    EXPECT_EQ(cfg.routes[0].target, "auth");
    EXPECT_FALSE(cfg.routes[0].upgrade_websocket);
    
    EXPECT_EQ(cfg.routes[1].match_pattern, "^/v1/rt/connect$");
    EXPECT_EQ(cfg.routes[1].target, "messaging");
    EXPECT_TRUE(cfg.routes[1].upgrade_websocket);
    
    EXPECT_EQ(cfg.routes[2].match_pattern, ".*");
    EXPECT_EQ(cfg.routes[2].target, "files");
}

TEST_F(FullConfigTest, ParsesUpstreams) {
    create_full_config();
    
    GatewayConfig cfg = load_gateway_config(test_config_path);
    
    ASSERT_EQ(cfg.upstreams.size(), 3);
    
    // Find auth upstream
    auto auth_it = std::find_if(cfg.upstreams.begin(), cfg.upstreams.end(),
        [](const UpstreamConfig& u) { return u.name == "auth"; });
    ASSERT_NE(auth_it, cfg.upstreams.end());
    EXPECT_EQ(auth_it->kind, UpstreamKind::HTTP);
    EXPECT_EQ(auth_it->transport, UpstreamTransport::UDS);
    EXPECT_EQ(auth_it->address, "/run/securecloud/auth.sock");
    
    // Find messaging upstream
    auto msg_it = std::find_if(cfg.upstreams.begin(), cfg.upstreams.end(),
        [](const UpstreamConfig& u) { return u.name == "messaging"; });
    ASSERT_NE(msg_it, cfg.upstreams.end());
    EXPECT_EQ(msg_it->kind, UpstreamKind::WS);
    EXPECT_EQ(msg_it->transport, UpstreamTransport::TCP);
    EXPECT_EQ(msg_it->address, "messaging:8081");
}

TEST_F(FullConfigTest, ParsesSecurity) {
    create_full_config();
    
    GatewayConfig cfg = load_gateway_config(test_config_path);
    
    EXPECT_EQ(cfg.security.jwks_cache_ttl_s, 600);
}

TEST_F(FullConfigTest, ParsesObservability) {
    create_full_config();
    
    GatewayConfig cfg = load_gateway_config(test_config_path);
    
    EXPECT_EQ(cfg.observability.prometheus.bind_address, "0.0.0.0:9090");
    EXPECT_EQ(cfg.observability.logs.level, "debug");
}

TEST_F(FullConfigTest, DefaultsWhenFileNotFound) {
    GatewayConfig cfg = load_gateway_config("nonexistent.yaml");
    
    EXPECT_EQ(cfg.server.host, "127.0.0.1");
    EXPECT_EQ(cfg.server.port, 8080);
    EXPECT_TRUE(cfg.routes.empty());
    EXPECT_TRUE(cfg.upstreams.empty());
}

TEST_F(FullConfigTest, EnvironmentVariantLoading) {
    // Create a prod config variant
    std::string prod_path = "test_full_gateway.prod.yaml";
    std::ofstream out(prod_path);
    out << "server:\n  host: prod.example.com\n  port: 443\n";
    out.close();
    
    GatewayConfig cfg = load_gateway_config("test_full_gateway.dev.yaml", "prod");
    
    EXPECT_EQ(cfg.server.host, "prod.example.com");
    EXPECT_EQ(cfg.server.port, 443);
    EXPECT_EQ(cfg.environment, "prod");
    
    std::remove(prod_path.c_str());
}
