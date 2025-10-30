#pragma once
#include "gateway/types.hpp"


namespace gateway {
class UpstreamProxy {
public:
Response forwardHttp(const Request& r, const UpstreamTarget& t) const;
};
} // namespace gateway