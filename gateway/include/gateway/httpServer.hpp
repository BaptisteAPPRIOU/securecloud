#pragma once
#include "gateway/types.hpp"
#include <string>


namespace gateway {
class HttpServer {
public:
HttpServer();
void onRequest(RequestHandler handler);
void start();
private:
RequestHandler handler_{};
};
} // namespace gateway