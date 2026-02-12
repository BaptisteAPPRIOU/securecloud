#pragma once
#include "types.hpp"
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace gateway {

class TLSContext;

struct ClientConnection {
    void* socket;
    std::string client_ip;
};

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    int thread_pool_size = 4;
    int request_timeout_ms = 30000;
    size_t max_request_size = 1024 * 1024; // 1MB
};

class HttpServer {
public:
    HttpServer();
    explicit HttpServer(const ServerConfig& config);
    ~HttpServer();
    void onRequest(RequestHandler handler);
    void configure_tls(const std::string& cert_file, const std::string& key_file, bool client_mtls = false);
    bool is_tls_enabled() const { return tls_enabled_; }
    void start();
    void stop();
    
private:
    RequestHandler handler_{};
    ServerConfig config_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;
    std::queue<ClientConnection> connection_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::unique_ptr<TLSContext> tls_context_;
    bool tls_enabled_{false};
    
    void worker_thread_fn();
    void handle_client(const ClientConnection& conn);
    Request parse_http_request(const std::string& raw_request, const std::string& client_ip);
    std::string format_http_response(const Response& resp);
};

} // namespace gateway
