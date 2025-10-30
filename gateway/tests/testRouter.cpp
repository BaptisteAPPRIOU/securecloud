#include "gateway/router.hpp"
#include <gtest/gtest.h>
using namespace gateway;


TEST(Router, PicksAuthForAuthPaths) {
Router r;
Request req{.method="GET", .path="/v1/auth/validate"};
auto t = r.route(req);
EXPECT_EQ(t.name, "auth");
}