#include "restApiEndpoints.hpp"
#include <spdlog/spdlog.h>

namespace gateway {

RestApiEndpoints::RestApiEndpoints(UpstreamProxy& proxy)
    : upstream_proxy_(proxy) {
    spdlog::info("RestApiEndpoints initialized");
}

// ============================================================================
// Helper Methods
// ============================================================================

Response RestApiEndpoints::error_response(int status, const std::string& error_code, 
                                         const std::string& message) const {
    nlohmann::json error_json = {
        {"error", error_code},
        {"message", message}
    };
    
    Response resp;
    resp.status = status;
    resp.body = error_json.dump();
    resp.headers["Content-Type"] = "application/json";
    return resp;
}

Response RestApiEndpoints::json_response(int status, const nlohmann::json& data) const {
    Response resp;
    resp.status = status;
    resp.body = data.dump();
    resp.headers["Content-Type"] = "application/json";
    return resp;
}

std::optional<nlohmann::json> RestApiEndpoints::parse_json_body(const Request& r) const {
    try {
        if (r.body.empty()) {
            return std::nullopt;
        }
        return nlohmann::json::parse(r.body);
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("JSON parse error: {}", e.what());
        return std::nullopt;
    }
}

std::string RestApiEndpoints::extract_path_param(const std::string& path, 
                                                 const std::string& prefix) const {
    // Extract parameter after prefix (e.g., "/api/files/file_123" -> "file_123")
    if (path.size() > prefix.size() && path.find(prefix) == 0) {
        return path.substr(prefix.size());
    }
    return "";
}

// ============================================================================
// Authentication Endpoints
// ============================================================================

Response RestApiEndpoints::handle_login(const Request& r) {
    spdlog::info("POST /api/login");
    
    auto json_opt = parse_json_body(r);
    if (!json_opt) {
        return error_response(400, "invalid_request", "Invalid JSON body");
    }
    
    const auto& req_json = *json_opt;
    
    if (!req_json.contains("username") || !req_json.contains("password")) {
        return error_response(400, "invalid_request", "Missing username or password");
    }
    
    // Forward to auth-service
    Request auth_req = r;
    auth_req.path = "/auth/login";
    
    Response auth_resp = upstream_proxy_.forward(auth_req, "auth-service");
    
    // Transform response for Qt client
    if (auth_resp.status == 200) {
        try {
            auto auth_json = nlohmann::json::parse(auth_resp.body);
            
            // Extract tokens and user info
            nlohmann::json response = {
                {"access_token", auth_json.value("access_token", "")},
                {"refresh_token", auth_json.value("refresh_token", "")},
                {"expires_in", auth_json.value("expires_in", 3600)},
                {"token_type", "Bearer"}
            };
            
            if (auth_json.contains("user")) {
                response["user"] = auth_json["user"];
            }
            
            return json_response(200, response);
            
        } catch (const nlohmann::json::exception& e) {
            spdlog::error("Failed to parse auth-service response: {}", e.what());
            return error_response(500, "internal_error", "Authentication failed");
        }
    }
    
    // Pass through error responses
    return auth_resp;
}

Response RestApiEndpoints::handle_refresh(const Request& r) {
    spdlog::info("POST /api/refresh");
    
    auto json_opt = parse_json_body(r);
    if (!json_opt) {
        return error_response(400, "invalid_request", "Invalid JSON body");
    }
    
    const auto& req_json = *json_opt;
    
    if (!req_json.contains("refresh_token")) {
        return error_response(400, "invalid_request", "Missing refresh_token");
    }
    
    // Forward to auth-service
    Request auth_req = r;
    auth_req.path = "/auth/refresh";
    
    Response auth_resp = upstream_proxy_.forward(auth_req, "auth-service");
    
    // Transform response
    if (auth_resp.status == 200) {
        try {
            auto auth_json = nlohmann::json::parse(auth_resp.body);
            
            nlohmann::json response = {
                {"access_token", auth_json.value("access_token", "")},
                {"expires_in", auth_json.value("expires_in", 3600)},
                {"token_type", "Bearer"}
            };
            
            return json_response(200, response);
            
        } catch (const nlohmann::json::exception& e) {
            spdlog::error("Failed to parse refresh response: {}", e.what());
            return error_response(500, "internal_error", "Token refresh failed");
        }
    }
    
    return auth_resp;
}

Response RestApiEndpoints::handle_logout(const Request& r) {
    spdlog::info("POST /api/logout");
    
    // Forward to auth-service
    Request auth_req = r;
    auth_req.path = "/auth/logout";
    
    Response auth_resp = upstream_proxy_.forward(auth_req, "auth-service");
    
    if (auth_resp.status == 200) {
        return json_response(200, {{"success", true}});
    }
    
    return auth_resp;
}

// ============================================================================
// User Profile Endpoints
// ============================================================================

Response RestApiEndpoints::handle_get_profile(const Request& r, const Claims& claims) {
    spdlog::info("GET /api/me user={}", claims.sub);
    
    // Forward to auth-service
    Request auth_req = r;
    auth_req.path = "/auth/users/" + claims.sub;
    
    Response auth_resp = upstream_proxy_.forward(auth_req, "auth-service");
    
    // Return profile data
    return auth_resp;
}

// ============================================================================
// Messaging Endpoints
// ============================================================================

Response RestApiEndpoints::handle_get_conversations(const Request& r, const Claims& claims) {
    spdlog::info("GET /api/conversations user={}", claims.sub);
    
    // Forward to messaging-service
    Request msg_req = r;
    msg_req.path = "/messaging/conversations";
    msg_req.headers["X-User-ID"] = claims.sub; // Pass user ID to microservice
    
    Response msg_resp = upstream_proxy_.forward(msg_req, "messaging-service");
    
    // Transform response for Qt client (if needed)
    return msg_resp;
}

Response RestApiEndpoints::handle_get_messages(const Request& r, const Claims& claims) {
    // Extract conversation ID from path (/api/conversations/{id}/messages)
    std::string conv_id = extract_path_param(r.path, "/api/conversations/");
    
    // Remove "/messages" suffix
    size_t slash_pos = conv_id.find('/');
    if (slash_pos != std::string::npos) {
        conv_id = conv_id.substr(0, slash_pos);
    }
    
    spdlog::info("GET /api/conversations/{}/messages user={}", conv_id, claims.sub);
    
    // Forward to messaging-service
    Request msg_req = r;
    msg_req.path = "/messaging/conversations/" + conv_id + "/messages";
    msg_req.headers["X-User-ID"] = claims.sub;
    
    Response msg_resp = upstream_proxy_.forward(msg_req, "messaging-service");
    
    return msg_resp;
}

// ============================================================================
// File Endpoints
// ============================================================================

Response RestApiEndpoints::handle_upload_file(const Request& r, const Claims& claims) {
    spdlog::info("POST /api/files user={}", claims.sub);
    
    // Forward to file-service
    Request file_req = r;
    file_req.path = "/files/upload";
    file_req.headers["X-User-ID"] = claims.sub;
    
    Response file_resp = upstream_proxy_.forward(file_req, "file-service");
    
    // Transform response
    if (file_resp.status == 201 || file_resp.status == 200) {
        try {
            auto file_json = nlohmann::json::parse(file_resp.body);
            
            nlohmann::json response = {
                {"file_id", file_json.value("id", "")},
                {"filename", file_json.value("filename", "")},
                {"size", file_json.value("size", 0)},
                {"content_type", file_json.value("content_type", "application/octet-stream")},
                {"url", "/api/files/" + file_json.value("id", "")}
            };
            
            return json_response(201, response);
            
        } catch (const nlohmann::json::exception& e) {
            spdlog::error("Failed to parse file upload response: {}", e.what());
        }
    }
    
    return file_resp;
}

Response RestApiEndpoints::handle_download_file(const Request& r, const Claims& claims) {
    // Extract file ID from path
    std::string file_id = extract_path_param(r.path, "/api/files/");
    
    spdlog::info("GET /api/files/{} user={}", file_id, claims.sub);
    
    // Forward to file-service
    Request file_req = r;
    file_req.path = "/files/" + file_id;
    file_req.headers["X-User-ID"] = claims.sub;
    
    Response file_resp = upstream_proxy_.forward(file_req, "file-service");
    
    // Pass through file download response (binary data)
    return file_resp;
}

} // namespace gateway
