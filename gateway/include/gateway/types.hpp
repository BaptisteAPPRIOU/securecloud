#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <optional>


namespace gateway {
struct Request {
std::string method;
std::string path;
std::unordered_map<std::string, std::string> headers;
std::string body;
};


struct Response {
int status{200};
std::string body;
std::unordered_map<std::string, std::string> headers;
};


struct Claims {
std::string sub; // subject
std::unordered_map<std::string, std::string> values;
};


struct UpstreamTarget {
std::string name; // ex: auth, files, messaging
std::string address; // ex: /run/securecloud/auth.sock ou host:port
bool websocket{false};
};


using RequestHandler = std::function<Response(const Request&)>;
} // namespace gateway