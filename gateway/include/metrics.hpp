#pragma once
#include <string>


namespace gateway {
class Metrics {
public:
void incCounter(const std::string& name) const;
};
} // namespace gateway