#include "gateway/auditSink.hpp"
#include <spdlog/spdlog.h>


namespace gateway {
void AuditSink::record(const Request& r, const UpstreamTarget& t) const {
spdlog::info("[kit] audit {} -> {}", r.path, t.name);
}
} // namespace gateway