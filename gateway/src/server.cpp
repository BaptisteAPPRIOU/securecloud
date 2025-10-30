#include "gateway/httpServer.hpp"
#include <spdlog/spdlog.h>


namespace gateway {
HttpServer::HttpServer() = default;


void HttpServer::onRequest(RequestHandler handler) { handler_ = std::move(handler); }


void HttpServer::start() {
spdlog::info("[kit] HttpServer started (stub). Expose real network later.");
// Stub: pas d'I/O réseau. Sert uniquement de squelette.
}
} // namespace gateway