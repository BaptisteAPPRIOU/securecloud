#include "udsClient.hpp"
#include <spdlog/spdlog.h>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>  // For htonl/ntohl on Windows
#else
#include <arpa/inet.h> // For htonl/ntohl on Unix
#endif

namespace gateway {

UdsClient::UdsClient(net::io_context& io_ctx,
                     const std::string& socket_path,
                     std::chrono::milliseconds timeout_ms)
    : io_context_(io_ctx), socket_path_(socket_path), timeout_(timeout_ms) {
    spdlog::debug("UdsClient created for socket: {}", socket_path_);
}

UdsClient::~UdsClient() {
    close();
}

void UdsClient::connect() {
    if (connected_) {
        return;
    }

    try {
        // Create Unix socket
        socket_ = std::make_unique<unix_socket::socket>(io_context_);
        
        // Connect to socket path
        unix_socket::endpoint endpoint(socket_path_);
        socket_->connect(endpoint);
        
        connected_ = true;
        spdlog::info("Connected to Unix socket: {}", socket_path_);
        
    } catch (const std::exception& e) {
        connected_ = false;
        spdlog::error("Failed to connect to Unix socket {}: {}", socket_path_, e.what());
        throw;
    }
}

void UdsClient::ensure_connected() {
    if (!connected_ || !socket_ || !socket_->is_open()) {
        connect();
    }
}

void UdsClient::close() {
    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->shutdown(unix_socket::socket::shutdown_both, ec);
        socket_->close(ec);
        connected_ = false;
        spdlog::debug("Closed Unix socket: {}", socket_path_);
    }
}

bool UdsClient::is_connected() const {
    return connected_ && socket_ && socket_->is_open();
}

bool UdsClient::send_message(const std::string& message) {
    try {
        // Protocol: [4-byte length][message]
        uint32_t length = htonl(static_cast<uint32_t>(message.size()));
        
        // Send length prefix
        boost::asio::write(*socket_, boost::asio::buffer(&length, sizeof(length)));
        
        // Send message body
        boost::asio::write(*socket_, boost::asio::buffer(message));
        
        spdlog::debug("Sent UDS message: {} bytes", message.size());
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to send UDS message: {}", e.what());
        close();
        return false;
    }
}

std::optional<std::string> UdsClient::receive_message() {
    try {
        // Read 4-byte length prefix
        uint32_t length_network;
        boost::asio::read(*socket_, boost::asio::buffer(&length_network, sizeof(length_network)));
        
        uint32_t length = ntohl(length_network);
        
        // Validate length (prevent memory exhaustion attacks)
        if (length == 0 || length > 10 * 1024 * 1024) { // Max 10MB
            spdlog::error("Invalid message length: {}", length);
            return std::nullopt;
        }
        
        // Read message body
        std::string message(length, '\0');
        boost::asio::read(*socket_, boost::asio::buffer(message.data(), length));
        
        spdlog::debug("Received UDS message: {} bytes", length);
        return message;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to receive UDS message: {}", e.what());
        close();
        return std::nullopt;
    }
}

std::optional<nlohmann::json> UdsClient::send_request(const nlohmann::json& request) {
    try {
        ensure_connected();
        
        // Serialize request to JSON string
        std::string request_str = request.dump();
        
        spdlog::debug("Sending UDS request: {}", request_str);
        
        // Send request
        if (!send_message(request_str)) {
            return std::nullopt;
        }
        
        // Receive response
        auto response_str = receive_message();
        if (!response_str) {
            return std::nullopt;
        }
        
        // Parse JSON response
        nlohmann::json response = nlohmann::json::parse(*response_str);
        
        spdlog::debug("Received UDS response: {}", response.dump());
        
        return response;
        
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error in UDS communication: {}", e.what());
        return std::nullopt;
        
    } catch (const std::exception& e) {
        spdlog::error("UDS request failed: {}", e.what());
        close();
        return std::nullopt;
    }
}

std::optional<std::string> UdsClient::send_raw(const std::string& request) {
    try {
        ensure_connected();
        
        if (!send_message(request)) {
            return std::nullopt;
        }
        
        return receive_message();
        
    } catch (const std::exception& e) {
        spdlog::error("UDS raw request failed: {}", e.what());
        close();
        return std::nullopt;
    }
}

} // namespace gateway
