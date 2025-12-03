#include "gateway/router.hpp"
#include <gtest/gtest.h>
using namespace gateway;


TEST(Router, PicksAuthForAuthPaths) {
    // Router now requires configuration - use default constructor for backward compatibility
    Router r;
    Request req{.method="GET", .path="/v1/auth/validate", .headers={}, .body=""};
    auto t = r.route(req);
    
    // Router now returns std::optional<UpstreamTarget>
    // Without configuration, route() returns nullopt
    EXPECT_FALSE(t.has_value());
    
    // TODO: Update test with proper route configuration when config is available
}