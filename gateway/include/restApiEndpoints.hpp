#pragma once

#include "types.hpp"
#include "upstreamProxy.hpp"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace gateway {

/**
 * REST API Endpoints for Qt Client
 * 
 * Implements authentication and business endpoints with:
 * - Token-based authentication (JWT)
 * - Response transformation for UI consumption
 * - Error handling with proper HTTP status codes
 * - Microservice integration via UpstreamProxy
 */
class RestApiEndpoints {
public:
    RestApiEndpoints(UpstreamProxy& proxy);
    
    // ========================================================================
    // Authentication Endpoints
    // ========================================================================
    
    /**
     * POST /api/login
     * Authenticate user and return access/refresh tokens
     * 
     * Request body:
     * {
     *   "email": "user@example.com",
     *   "password": "secretpass123"
     * }
     * 
     * Response (200 OK):
     * {
     *   "access_token": "eyJhbGciOi...",
     *   "refresh_token": "eyJhbGciOi...",
     *   "expires_in": 3600,
     *   "token_type": "Bearer",
     *   "user": {
     *     "id": "user_123",
     *     "email": "user@example.com",
     *     "name": "John Doe"
     *   }
     * }
     * 
     * Response (401 Unauthorized):
     * {"error": "invalid_credentials", "message": "Invalid email or password"}
     */
    Response handle_login(const Request& r);
    
    /**
     * POST /api/refresh
     * Refresh access token using refresh token
     * 
     * Request body:
     * {
     *   "refresh_token": "eyJhbGciOi..."
     * }
     * 
     * Response (200 OK):
     * {
     *   "access_token": "eyJhbGciOi...",
     *   "expires_in": 3600,
     *   "token_type": "Bearer"
     * }
     */
    Response handle_refresh(const Request& r);
    
    /**
     * POST /api/logout
     * Invalidate current session and tokens
     * 
     * Request headers:
     *   Authorization: Bearer <access_token>
     * 
     * Response (200 OK):
     * {"success": true}
     */
    Response handle_logout(const Request& r);
    
    // ========================================================================
    // User Profile Endpoints
    // ========================================================================
    
    /**
     * GET /api/me
     * Get current user profile
     * 
     * Request headers:
     *   Authorization: Bearer <access_token>
     * 
     * Response (200 OK):
     * {
     *   "id": "user_123",
     *   "email": "user@example.com",
     *   "name": "John Doe",
     *   "avatar_url": "https://...",
     *   "created_at": "2025-01-01T00:00:00Z"
     * }
     */
    Response handle_get_profile(const Request& r, const Claims& claims);
    
    // ========================================================================
    // Messaging Endpoints
    // ========================================================================
    
    /**
     * GET /api/conversations
     * Get list of user's conversations
     * 
     * Query params:
     *   ?limit=20&offset=0
     * 
     * Response (200 OK):
     * {
     *   "conversations": [
     *     {
     *       "id": "conv_123",
     *       "title": "Team Discussion",
     *       "participants": [...],
     *       "last_message": {...},
     *       "unread_count": 3,
     *       "updated_at": "2025-12-03T10:30:00Z"
     *     }
     *   ],
     *   "total": 42,
     *   "limit": 20,
     *   "offset": 0
     * }
     */
    Response handle_get_conversations(const Request& r, const Claims& claims);
    
    /**
     * GET /api/conversations/{id}/messages
     * Get messages for a specific conversation
     * 
     * Query params:
     *   ?limit=50&before=msg_456
     * 
     * Response (200 OK):
     * {
     *   "messages": [
     *     {
     *       "id": "msg_789",
     *       "conversation_id": "conv_123",
     *       "sender_id": "user_456",
     *       "sender_name": "Jane Doe",
     *       "content": "Hello!",
     *       "timestamp": "2025-12-03T10:25:00Z",
     *       "read": true
     *     }
     *   ],
     *   "has_more": true
     * }
     */
    Response handle_get_messages(const Request& r, const Claims& claims);
    
    // ========================================================================
    // File Endpoints
    // ========================================================================
    
    /**
     * POST /api/files
     * Upload file
     * 
     * Request:
     *   Content-Type: multipart/form-data
     *   Body: file binary data
     * 
     * Response (201 Created):
     * {
     *   "file_id": "file_123",
     *   "filename": "document.pdf",
     *   "size": 1048576,
     *   "content_type": "application/pdf",
     *   "url": "/api/files/file_123"
     * }
     */
    Response handle_upload_file(const Request& r, const Claims& claims);
    
    /**
     * GET /api/files/{id}
     * Download file
     * 
     * Response (200 OK):
     *   Content-Type: application/octet-stream
     *   Content-Disposition: attachment; filename="document.pdf"
     *   Body: file binary data
     */
    Response handle_download_file(const Request& r, const Claims& claims);

private:
    UpstreamProxy& upstream_proxy_;
    
    // Helper methods
    Response error_response(int status, const std::string& error_code, 
                           const std::string& message) const;
    Response json_response(int status, const nlohmann::json& data) const;
    std::optional<nlohmann::json> parse_json_body(const Request& r) const;
    std::string extract_path_param(const std::string& path, const std::string& prefix) const;
};

} // namespace gateway
