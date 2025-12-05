#include "httpServer.hpp"
#include "requestContext.hpp"
#include <spdlog/spdlog.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <csignal>
#include <sstream>
#include <algorithm>
#include <cstdint>

namespace gateway {

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
    SOCKET client_socket = static_cast<SOCKET>(reinterpret_cast<uintptr_t>(conn.socket));
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
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

    closesocket(client_socket);
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
    // Register minimal signal handler for Windows (SIGINT only).
    // Handler only sets `should_exit` to keep it safe.
    std::signal(SIGINT, signal_handler);

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        spdlog::error("WSAStartup failed");
        return;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        spdlog::error("Socket creation failed");
        WSACleanup();
        return;
    }

    // Enable SO_REUSEADDR to allow reuse of the port
    int reuse = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        spdlog::warn("setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(config_.host.c_str());
    server_addr.sin_port = htons(config_.port);

    if (bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        spdlog::error("Bind failed on {}:{}", config_.host, config_.port);
        closesocket(listen_socket);
        WSACleanup();
        return;
    }
    spdlog::info("Socket bound successfully on {}:{}", config_.host, config_.port);

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        spdlog::error("Listen failed");
        closesocket(listen_socket);
        WSACleanup();
        return;
    }

    running_ = true;

    // Start worker threads
    for (int i = 0; i < config_.thread_pool_size; ++i) {
        worker_threads_.emplace_back([this] { worker_thread_fn(); });
    }

    spdlog::info("HttpServer listening on {}:{} with {} worker threads",
                 config_.host, config_.port, config_.thread_pool_size);

    // Accept loop
    while (running_ && !should_exit) {
        sockaddr_in client_addr = {};
        int client_addr_len = sizeof(client_addr);

        // Set accept timeout
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        SOCKET client_socket = accept(listen_socket, (sockaddr*)&client_addr, &client_addr_len);

        if (client_socket == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err != WSAETIMEDOUT && err != WSAEWOULDBLOCK) {
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

    closesocket(listen_socket);
    stop();
    WSACleanup();
    spdlog::info("HttpServer stopped");
}

} // namespace gateway