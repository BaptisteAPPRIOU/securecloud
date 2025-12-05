#include <gtest/gtest.h>
#include "httpServer.hpp"

using namespace gateway;

// Minimal test: ensure ServerConfig can be passed and server created (without starting sockets)
TEST(ServerTest, ConstructWithConfig) {
    ServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 8081; // choose a different port to avoid conflicts
    cfg.thread_pool_size = 2;

    HttpServer server(cfg);
    // register a trivial handler
    server.onRequest([](const Request& r)->Response{
        return {200, "ok", {}};
    });

    // We won't call start() (bind/listen) in unit tests here to avoid network operations.
    SUCCEED();
}
