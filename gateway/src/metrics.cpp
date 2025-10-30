#include "gateway/metrics.hpp"
#include <spdlog/spdlog.h>


namespace gateway {
void Metrics::incCounter(const std::string& name) const {
spdlog::info("[kit] metric++ {}", name);
}
} // namespace gateway