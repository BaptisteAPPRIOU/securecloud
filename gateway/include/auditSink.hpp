#pragma once
#include "types.hpp"


namespace gateway {
class AuditSink {
public:
void record(const Request& r, const UpstreamTarget& t) const;
};
} // namespace gateway