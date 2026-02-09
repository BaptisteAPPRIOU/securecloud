#pragma once
#include "types.hpp"
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace gateway {

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
    
    void worker_thread_fn();
    void handle_client(const ClientConnection& conn);
    Request parse_http_request(const std::string& raw_request, const std::string& client_ip);
    std::string format_http_response(const Response& resp);
};

} // namespace gateway