#include "httpServer.hpp"
#include "requestContext.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <csignal>
#include <sstream>
#include <algorithm>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace gateway {

namespace {

#ifdef _WIN32
using socket_handle = SOCKET;
constexpr socket_handle kInvalidSocket = INVALID_SOCKET;
constexpr int kSocketError = SOCKET_ERROR;

bool initialize_socket_runtime() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        spdlog::error("WSAStartup failed");
        return false;
    }
    return true;
}

void cleanup_socket_runtime() {
    WSACleanup();
}

int last_socket_error() {
    return WSAGetLastError();
}

bool is_timeout_error(int err) {
    return err == WSAETIMEDOUT || err == WSAEWOULDBLOCK;
}
#else
using socket_handle = int;
constexpr socket_handle kInvalidSocket = -1;
constexpr int kSocketError = -1;

bool initialize_socket_runtime() {
    return true;
}

void cleanup_socket_runtime() {
}

int last_socket_error() {
    return errno;
}

bool is_timeout_error(int err) {
    return err == EAGAIN || err == EWOULDBLOCK;
}
#endif

void close_socket(socket_handle socket_fd) {
#ifdef _WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
}

} // namespace

static std::atomic<bool> should_exit(false);
static HttpServer* g_server_instance = nullptr;

void signal_handler(int signal) {
    // Only set the atomic flag inside the signal handler.
    // Avoid calling non-reentrant functions from the signal handler.
    if (signal == SIGINT) {
        should_exit = true;
    }
}

HttpServer::HttpServer()
    : config_{} {
}

HttpServer::HttpServer(const ServerConfig& config)
    : config_(config) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::onRequest(RequestHandler handler) {
    handler_ = std::move(handler);
}

void HttpServer::stop() {
    running_ = false;
    queue_cv_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

Request HttpServer::parse_http_request(const std::string& raw_request, const std::string& client_ip) {
    Request req;
    req.context = std::make_shared<RequestContext>(client_ip);
    
    std::istringstream stream(raw_request);
    std::string line;
    bool first_line = true;

    // Parse request line: METHOD PATH HTTP/VERSION
    if (std::getline(stream, line)) {
        line.erase(line.find_last_not_of("\r\n") + 1);
        if (first_line) {
            std::istringstream line_stream(line);
            std::string http_version;
            line_stream >> req.method >> req.path >> http_version;
            first_line = false;
        }
    }

    // Parse headers
    while (std::getline(stream, line)) {
        line.erase(line.find_last_not_of("\r\n") + 1);
        if (line.empty()) break; // End of headers

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            req.headers[key] = value;
        }
    }

    // Set correlation ID from X-Correlation-ID header if present
    auto corr_it = req.headers.find("x-correlation-id");
    if (corr_it == req.headers.end()) {
        corr_it = req.headers.find("X-Correlation-ID");
    }
    if (corr_it != req.headers.end()) {
        req.context->set_correlation_id(corr_it->second);
    }

    // Parse body
    std::string body_line;
    while (std::getline(stream, body_line)) {
        if (!req.body.empty()) req.body += "\n";
        req.body += body_line;
    }

    return req;
}

std::string HttpServer::format_http_response(const Response& resp) {
    std::ostringstream response;
    response << "HTTP/1.1 " << resp.status << " ";
    
    // Add status text
    switch (resp.status) {
        case 200: response << "OK"; break;
        case 400: response << "Bad Request"; break;
        case 401: response << "Unauthorized"; break;
        case 403: response << "Forbidden"; break;
        case 404: response << "Not Found"; break;
        case 500: response << "Internal Server Error"; break;
        default: response << "OK"; break;
    }
    response << "\r\n";

    // Add headers
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << resp.body.size() << "\r\n";
    response << "Connection: close\r\n";
    
    for (const auto& [key, value] : resp.headers) {
        response << key << ": " << value << "\r\n";
    }
    
    response << "\r\n";
    response << resp.body;
    
    return response.str();
}

void HttpServer::handle_client(const ClientConnection& conn) {
    socket_handle client_socket = static_cast<socket_handle>(reinterpret_cast<uintptr_t>(conn.socket));
    constexpr int kBufferSize = 4096;
    char buffer[kBufferSize];
    auto bytes_received = recv(client_socket, buffer, kBufferSize - 1, 0);

    if (bytes_received > 0) {
        buffer[static_cast<size_t>(bytes_received)] = '\0';
        std::string raw_request(buffer);

        try {
            Request req = parse_http_request(raw_request, conn.client_ip);
            spdlog::info("{} {} {} {}", req.context->format(), req.method, req.path, conn.client_ip);

            Response resp = {500, "Handler not configured", {}};
            if (handler_) {
                resp = handler_(req);
            }

            std::string http_response = format_http_response(resp);
            send(client_socket, http_response.c_str(), static_cast<int>(http_response.size()), 0);
        } catch (const std::exception& e) {
            spdlog::error("Request handling error: {}", e.what());
            Response error_resp{500, "Internal Server Error", {}};
            std::string error_response = format_http_response(error_resp);
            send(client_socket, error_response.c_str(), static_cast<int>(error_response.size()), 0);
        }
    }

    close_socket(client_socket);
}

void HttpServer::worker_thread_fn() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !connection_queue_.empty() || !running_; });

        if (!running_) break;

        if (!connection_queue_.empty()) {
            ClientConnection conn = connection_queue_.front();
            connection_queue_.pop();
            lock.unlock();

            handle_client(conn);
        }
    }
}

void HttpServer::start() {
    g_server_instance = this;
    // Register minimal signal handler (SIGINT only).
    // Handler only sets `should_exit` to keep it safe.
    std::signal(SIGINT, signal_handler);

    if (!initialize_socket_runtime()) {
        return;
    }

    socket_handle listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == kInvalidSocket) {
        spdlog::error("Socket creation failed");
        cleanup_socket_runtime();
        return;
    }

    // Enable SO_REUSEADDR to allow reuse of the port
    int reuse = 1;
#ifdef _WIN32
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), static_cast<int>(sizeof(reuse))) < 0) {
#else
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
#endif
        spdlog::warn("setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(config_.host.c_str());
    server_addr.sin_port = htons(config_.port);

    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == kSocketError) {
        spdlog::error("Bind failed on {}:{}", config_.host, config_.port);
        close_socket(listen_socket);
        cleanup_socket_runtime();
        return;
    }
    spdlog::info("Socket bound successfully on {}:{}", config_.host, config_.port);

    if (listen(listen_socket, SOMAXCONN) == kSocketError) {
        spdlog::error("Listen failed");
        close_socket(listen_socket);
        cleanup_socket_runtime();
        return;
    }

    running_ = true;

    // Start worker threads
    for (int i = 0; i < config_.thread_pool_size; ++i) {
        worker_threads_.emplace_back([this] { worker_thread_fn(); });
    }

    spdlog::info("=== SecureCloud Gateway RUNNING ===");
    spdlog::info("Listening on http://{}:{}", config_.host, config_.port);
    spdlog::info("Worker threads: {}", config_.thread_pool_size);
    spdlog::info("Ready to handle requests...");
    spdlog::info("=====================================");

    // Accept loop
    while (running_ && !should_exit) {
        sockaddr_in client_addr = {};
#ifdef _WIN32
        int client_addr_len = sizeof(client_addr);
#else
        socklen_t client_addr_len = sizeof(client_addr);
#endif

        // Set accept timeout
        // Poll accept with a short timeout so SIGINT can stop the server quickly.
#ifdef _WIN32
        const int timeout_ms = 1000;
        setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), static_cast<int>(sizeof(timeout_ms)));
#else
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

        socket_handle client_socket = accept(listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);

        if (client_socket == kInvalidSocket) {
            int err = last_socket_error();
#ifdef _WIN32
            if (!is_timeout_error(err)) {
#else
            if (!is_timeout_error(err) && err != EINTR) {
#endif
                spdlog::debug("Accept error: {}", err);
            }
            continue;
        }

        spdlog::debug("Client connected from {}", inet_ntoa(client_addr.sin_addr));

        // Queue the connection with client IP for a worker thread
        ClientConnection conn{
            reinterpret_cast<void*>(static_cast<uintptr_t>(client_socket)),
            inet_ntoa(client_addr.sin_addr)
        };
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            connection_queue_.push(conn);
        }
        queue_cv_.notify_one();
    }

    close_socket(listen_socket);
    stop();
    cleanup_socket_runtime();
    spdlog::info("HttpServer stopped");
}

} // namespace gateway
