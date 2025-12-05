#include "prometheusExporter.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

namespace gateway {

// ============================================================================
// Metric Base Class
// ============================================================================

Metric::Metric(const std::string& name, const std::string& help, MetricType type)
    : name_(name), help_(help), type_(type) {
}

// ============================================================================
// Counter Implementation
// ============================================================================

Counter::Counter(const std::string& name, const std::string& help)
    : Metric(name, help, MetricType::COUNTER), value_(0.0) {
}

void Counter::increment(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

void Counter::increment(const std::map<std::string, std::string>& labels, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    labeled_values_[labels] += value;
}

double Counter::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

std::string Counter::render() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    oss << "# HELP " << name_ << " " << help_ << "\n";
    oss << "# TYPE " << name_ << " counter\n";
    
    // Unlabeled metric
    if (labeled_values_.empty()) {
        oss << name_ << " " << value_ << "\n";
    }
    
    // Labeled metrics
    for (const auto& [labels, value] : labeled_values_) {
        oss << name_ << "{";
        bool first = true;
        for (const auto& [key, val] : labels) {
            if (!first) oss << ",";
            oss << key << "=\"" << val << "\"";
            first = false;
        }
        oss << "} " << value << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// Gauge Implementation
// ============================================================================

Gauge::Gauge(const std::string& name, const std::string& help)
    : Metric(name, help, MetricType::GAUGE), value_(0.0) {
}

void Gauge::set(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
}

void Gauge::set(const std::map<std::string, std::string>& labels, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    labeled_values_[labels] = value;
}

void Gauge::increment(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

void Gauge::increment(const std::map<std::string, std::string>& labels, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    labeled_values_[labels] += value;
}

void Gauge::decrement(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ -= value;
}

void Gauge::decrement(const std::map<std::string, std::string>& labels, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    labeled_values_[labels] -= value;
}

double Gauge::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

std::string Gauge::render() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    oss << "# HELP " << name_ << " " << help_ << "\n";
    oss << "# TYPE " << name_ << " gauge\n";
    
    if (labeled_values_.empty()) {
        oss << name_ << " " << value_ << "\n";
    }
    
    for (const auto& [labels, value] : labeled_values_) {
        oss << name_ << "{";
        bool first = true;
        for (const auto& [key, val] : labels) {
            if (!first) oss << ",";
            oss << key << "=\"" << val << "\"";
            first = false;
        }
        oss << "} " << value << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// Histogram Implementation
// ============================================================================

std::vector<double> Histogram::default_buckets() {
    return {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
}

Histogram::Histogram(const std::string& name, const std::string& help,
                     const std::vector<double>& buckets)
    : Metric(name, help, MetricType::HISTOGRAM), buckets_(buckets) {
    
    // Ensure buckets are sorted
    std::sort(buckets_.begin(), buckets_.end());
    
    // Initialize unlabeled observation buckets
    unlabeled_.bucket_counts.resize(buckets_.size(), 0);
}

void Histogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    unlabeled_.sum += value;
    unlabeled_.count++;
    
    // Increment bucket counts (cumulative)
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            unlabeled_.bucket_counts[i]++;
        }
    }
}

void Histogram::observe(const std::map<std::string, std::string>& labels, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& obs = labeled_[labels];
    if (obs.bucket_counts.empty()) {
        obs.bucket_counts.resize(buckets_.size(), 0);
    }
    
    obs.sum += value;
    obs.count++;
    
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (value <= buckets_[i]) {
            obs.bucket_counts[i]++;
        }
    }
}

std::string Histogram::render() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    oss << "# HELP " << name_ << " " << help_ << "\n";
    oss << "# TYPE " << name_ << " histogram\n";
    
    // Unlabeled histogram
    if (labeled_.empty()) {
        for (size_t i = 0; i < buckets_.size(); ++i) {
            oss << name_ << "_bucket{le=\"" << buckets_[i] << "\"} " 
                << unlabeled_.bucket_counts[i] << "\n";
        }
        oss << name_ << "_bucket{le=\"+Inf\"} " << unlabeled_.count << "\n";
        oss << name_ << "_sum " << unlabeled_.sum << "\n";
        oss << name_ << "_count " << unlabeled_.count << "\n";
    }
    
    // Labeled histograms
    for (const auto& [labels, obs] : labeled_) {
        std::string label_str;
        for (const auto& [key, val] : labels) {
            if (!label_str.empty()) label_str += ",";
            label_str += key + "=\"" + val + "\"";
        }
        
        for (size_t i = 0; i < buckets_.size(); ++i) {
            oss << name_ << "_bucket{" << label_str << ",le=\"" << buckets_[i] 
                << "\"} " << obs.bucket_counts[i] << "\n";
        }
        oss << name_ << "_bucket{" << label_str << ",le=\"+Inf\"} " << obs.count << "\n";
        oss << name_ << "_sum{" << label_str << "} " << obs.sum << "\n";
        oss << name_ << "_count{" << label_str << "} " << obs.count << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// MetricsRegistry Implementation
// ============================================================================

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry registry;
    return registry;
}

std::shared_ptr<Counter> MetricsRegistry::register_counter(const std::string& name, 
                                                           const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<Counter>(it->second);
    }
    
    auto counter = std::make_shared<Counter>(name, help);
    metrics_[name] = counter;
    
    spdlog::debug("Registered counter metric: {}", name);
    return counter;
}

std::shared_ptr<Gauge> MetricsRegistry::register_gauge(const std::string& name,
                                                       const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<Gauge>(it->second);
    }
    
    auto gauge = std::make_shared<Gauge>(name, help);
    metrics_[name] = gauge;
    
    spdlog::debug("Registered gauge metric: {}", name);
    return gauge;
}

std::shared_ptr<Histogram> MetricsRegistry::register_histogram(const std::string& name,
                                                               const std::string& help,
                                                               const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return std::static_pointer_cast<Histogram>(it->second);
    }
    
    auto histogram = std::make_shared<Histogram>(name, help, buckets);
    metrics_[name] = histogram;
    
    spdlog::debug("Registered histogram metric: {}", name);
    return histogram;
}

std::string MetricsRegistry::render_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    for (const auto& [name, metric] : metrics_) {
        oss << metric->render();
    }
    
    return oss.str();
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

// ============================================================================
// PrometheusExporter Implementation
// ============================================================================

struct PrometheusExporter::Impl {
    int socket_fd{-1};
    std::string host;
    int port;
};

PrometheusExporter::PrometheusExporter(const std::string& bind_address)
    : bind_address_(bind_address), running_(false), impl_(std::make_unique<Impl>()) {
    
    // Parse bind_address (format: "host:port")
    size_t colon_pos = bind_address.find(':');
    if (colon_pos != std::string::npos) {
        impl_->host = bind_address.substr(0, colon_pos);
        impl_->port = std::stoi(bind_address.substr(colon_pos + 1));
    } else {
        impl_->host = "0.0.0.0";
        impl_->port = 9090;
    }
    
    spdlog::info("PrometheusExporter initialized - bind_address={}", bind_address_);
}

PrometheusExporter::~PrometheusExporter() {
    stop();
}

void PrometheusExporter::start() {
    if (running_) {
        spdlog::warn("PrometheusExporter already running");
        return;
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("Failed to initialize Winsock");
        return;
    }
#endif
    
    // Create socket
    impl_->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->socket_fd < 0) {
        spdlog::error("Failed to create metrics socket");
        return;
    }
    
    // Set socket options (allow address reuse)
    int opt = 1;
    setsockopt(impl_->socket_fd, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&opt), sizeof(opt));
    
    // Bind socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(impl_->port);
    addr.sin_addr.s_addr = inet_addr(impl_->host.c_str());
    
    if (bind(impl_->socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("Failed to bind metrics socket to {}:{}", impl_->host, impl_->port);
#ifdef _WIN32
        closesocket(impl_->socket_fd);
#else
        close(impl_->socket_fd);
#endif
        return;
    }
    
    // Listen for connections
    if (listen(impl_->socket_fd, 10) < 0) {
        spdlog::error("Failed to listen on metrics socket");
#ifdef _WIN32
        closesocket(impl_->socket_fd);
#else
        close(impl_->socket_fd);
#endif
        return;
    }
    
    running_ = true;
    spdlog::info("Prometheus metrics server started on {}:{}", impl_->host, impl_->port);
    
    // NOTE: In production, this should run in a separate thread
    // For now, it's a synchronous implementation (blocking accept)
    // TODO: Integrate with main io_context or use std::thread
}

void PrometheusExporter::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (impl_->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(impl_->socket_fd);
        WSACleanup();
#else
        close(impl_->socket_fd);
#endif
        impl_->socket_fd = -1;
    }
    
    spdlog::info("Prometheus metrics server stopped");
}

bool PrometheusExporter::is_running() const {
    return running_;
}

// ============================================================================
// HistogramTimer Implementation
// ============================================================================

HistogramTimer::HistogramTimer(std::shared_ptr<Histogram> histogram,
                               const std::map<std::string, std::string>& labels)
    : histogram_(histogram), labels_(labels), 
      start_(std::chrono::steady_clock::now()), stopped_(false) {
}

HistogramTimer::~HistogramTimer() {
    if (!stopped_) {
        stop();
    }
}

void HistogramTimer::stop() {
    if (stopped_) return;
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start_).count();
    
    if (labels_.empty()) {
        histogram_->observe(duration);
    } else {
        histogram_->observe(labels_, duration);
    }
    
    stopped_ = true;
}

} // namespace gateway
