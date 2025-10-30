#pragma once
#include "gateway/types.hpp"


namespace gateway {
class Router {
public:
Router() = default;
UpstreamTarget route(const Request& r) const;
};
} // namespace gateway