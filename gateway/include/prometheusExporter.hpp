#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>

namespace gateway {

/**
 * Prometheus Metric Types
 * https://prometheus.io/docs/concepts/metric_types/
 */
enum class MetricType {
    COUNTER,    // Monotonically increasing counter (requests_total, errors_total)
    GAUGE,      // Value that can go up and down (active_connections, memory_usage)
    HISTOGRAM,  // Observations bucketed by value (request_duration_seconds)
    SUMMARY     // Similar to histogram, calculates quantiles (not implemented yet)
};

/**
 * Base class for all Prometheus metrics
 */
class Metric {
public:
    Metric(const std::string& name, const std::string& help, MetricType type);
    virtual ~Metric() = default;
    
    virtual std::string render() const = 0;
    
    const std::string& name() const { return name_; }
    const std::string& help() const { return help_; }
    MetricType type() const { return type_; }

protected:
    std::string name_;
    std::string help_;
    MetricType type_;
    mutable std::mutex mutex_;
};

/**
 * Counter - monotonically increasing value
 * Use cases: total requests, total errors, bytes transferred
 */
class Counter : public Metric {
public:
    Counter(const std::string& name, const std::string& help);
    
    void increment(double value = 1.0);
    void increment(const std::map<std::string, std::string>& labels, double value = 1.0);
    
    double get() const;
    std::string render() const override;

private:
    double value_;
    std::map<std::map<std::string, std::string>, double> labeled_values_;
};

/**
 * Gauge - value that can increase or decrease
 * Use cases: active connections, memory usage, queue size
 */
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::string& help);
    
    void set(double value);
    void set(const std::map<std::string, std::string>& labels, double value);
    
    void increment(double value = 1.0);
    void increment(const std::map<std::string, std::string>& labels, double value = 1.0);
    
    void decrement(double value = 1.0);
    void decrement(const std::map<std::string, std::string>& labels, double value = 1.0);
    
    double get() const;
    std::string render() const override;

private:
    double value_;
    std::map<std::map<std::string, std::string>, double> labeled_values_;
};

/**
 * Histogram - observations bucketed by value
 * Use cases: request duration, response size
 * 
 * Buckets are cumulative (le = "less than or equal")
 * Default buckets: 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10
 */
class Histogram : public Metric {
public:
    Histogram(const std::string& name, const std::string& help,
              const std::vector<double>& buckets = default_buckets());
    
    void observe(double value);
    void observe(const std::map<std::string, std::string>& labels, double value);
    
    std::string render() const override;
    
    static std::vector<double> default_buckets();

private:
    struct Observation {
        std::vector<size_t> bucket_counts;
        size_t count{0};
        double sum{0.0};
    };
    
    std::vector<double> buckets_;
    Observation unlabeled_;
    std::map<std::map<std::string, std::string>, Observation> labeled_;
};

/**
 * Prometheus Metrics Registry
 * 
 * Central registry for all metrics. Thread-safe.
 * Metrics are registered once and accessed via shared_ptr.
 */
class MetricsRegistry {
public:
    static MetricsRegistry& instance();
    
    /**
     * Register or get existing counter
     */
    std::shared_ptr<Counter> register_counter(const std::string& name, const std::string& help);
    
    /**
     * Register or get existing gauge
     */
    std::shared_ptr<Gauge> register_gauge(const std::string& name, const std::string& help);
    
    /**
     * Register or get existing histogram
     */
    std::shared_ptr<Histogram> register_histogram(const std::string& name, const std::string& help,
                                                   const std::vector<double>& buckets = Histogram::default_buckets());
    
    /**
     * Render all metrics in Prometheus text format
     */
    std::string render_all() const;
    
    /**
     * Clear all metrics (for testing)
     */
    void clear();

private:
    MetricsRegistry() = default;
    
    std::map<std::string, std::shared_ptr<Metric>> metrics_;
    mutable std::mutex mutex_;
};

/**
 * Prometheus HTTP Server
 * 
 * Exposes /metrics endpoint on configured port (default: 9090)
 * Serves metrics in Prometheus text format
 */
class PrometheusExporter {
public:
    PrometheusExporter(const std::string& bind_address = "0.0.0.0:9090");
    ~PrometheusExporter();
    
    /**
     * Start metrics HTTP server
     */
    void start();
    
    /**
     * Stop metrics HTTP server
     */
    void stop();
    
    /**
     * Check if server is running
     */
    bool is_running() const;

private:
    std::string bind_address_;
    bool running_;
    
    // Forward declaration - implementation uses platform-specific socket API
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * RAII timer for measuring operation duration
 * 
 * Usage:
 *   auto timer = HistogramTimer(request_duration_histogram, {{"method", "GET"}});
 *   // ... do work ...
 *   // timer automatically records duration on destruction
 */
class HistogramTimer {
public:
    HistogramTimer(std::shared_ptr<Histogram> histogram,
                   const std::map<std::string, std::string>& labels = {});
    ~HistogramTimer();
    
    /**
     * Stop timer and record duration (called automatically on destruction)
     */
    void stop();

private:
    std::shared_ptr<Histogram> histogram_;
    std::map<std::string, std::string> labels_;
    std::chrono::steady_clock::time_point start_;
    bool stopped_;
};

} // namespace gateway
