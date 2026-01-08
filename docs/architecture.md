# SecureCloud Architecture

**Version:** 1.0  
**Last Updated:** January 6, 2026  
**Status:** MVP Phase - Production Readiness In Progress

## Table of Contents

1. [System Overview](#system-overview)
2. [Architecture Diagrams](#architecture-diagrams)
3. [Component Specifications](#component-specifications)
4. [API Contracts](#api-contracts)
5. [Data Flow & Communication](#data-flow--communication)
6. [Technology Stack](#technology-stack)
7. [Deployment Architecture](#deployment-architecture)
8. [Scalability & Performance](#scalability--performance)

---

## System Overview

SecureCloud is a **microservices-based secure file sharing and messaging platform** designed for high-security environments. The system provides end-to-end encrypted file storage, real-time messaging, and comprehensive audit logging with multi-tenancy support.

### Key Characteristics

- **Architecture Pattern**: Microservices with API Gateway
- **Communication**: RESTful HTTP + WebSockets (real-time messaging)
- **Security Model**: JWT-based authentication, RBAC authorization, TLS encryption
- **Data Persistence**: PostgreSQL with separate schemas per service
- **Deployment**: Docker Compose (dev/staging), Kubernetes-ready architecture
- **Observability**: Prometheus metrics, structured logging, distributed tracing

### Design Principles

1. **Defense in Depth**: Multiple security layers (TLS, JWT, RBAC, encryption at rest)
2. **Separation of Concerns**: Each microservice owns its domain and data
3. **Fail-Safe Defaults**: Deny-by-default authorization, rate limiting enabled
4. **Auditability**: All sensitive operations logged to audit service
5. **Multi-Tenancy**: Tenant isolation at database and application layers

---

## Architecture Diagrams

### High-Level System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Client Layer                            │
│  ┌──────────────────┐          ┌──────────────────┐            │
│  │   Qt Desktop     │          │   Web Client     │            │
│  │   Client         │          │   (Future)       │            │
│  └────────┬─────────┘          └────────┬─────────┘            │
│           │                              │                       │
└───────────┼──────────────────────────────┼───────────────────────┘
            │ HTTPS (8443)                 │
            │ HTTP  (8080)                 │
            │                              │
┌───────────▼──────────────────────────────▼───────────────────────┐
│                      Gateway Layer                                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              SecureCloud Gateway (:8080/:8443)          │    │
│  │                                                          │    │
│  │  • TLS Termination      • JWT Validation                │    │
│  │  • Dynamic Routing      • RBAC Authorization            │    │
│  │  • Rate Limiting        • WebSocket Proxy               │    │
│  │  • Request Correlation  • Metrics Export (:9090)        │    │
│  └─────────────────────────────────────────────────────────┘    │
└──────────┬────────────┬────────────┬────────────┬────────────────┘
           │            │            │            │
           │ HTTP       │ HTTP       │ HTTP       │ HTTP
           │ /auth/*    │ /files/*   │ /msg/*     │ /audit/*
           │            │            │            │
┌──────────▼────────────▼────────────▼────────────▼────────────────┐
│                    Microservices Layer                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │   Auth   │  │  Files   │  │Messaging │  │  Audit   │        │
│  │ Service  │  │ Service  │  │ Service  │  │ Service  │        │
│  │  :8001   │  │  :8003   │  │  :8004   │  │  :8002   │        │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘        │
│       │             │             │             │                 │
└───────┼─────────────┼─────────────┼─────────────┼─────────────────┘
        │             │             │             │
        │ SQL         │ SQL         │ SQL         │ SQL
        └─────────────┴─────────────┴─────────────┘
                      │
┌─────────────────────▼─────────────────────────────────────────────┐
│                      Data Layer                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │           PostgreSQL 16 (:15432)                         │    │
│  │                                                           │    │
│  │  Schemas:                                                │    │
│  │  • auth       (users, tenants, roles, sessions)         │    │
│  │  • files      (documents, permissions, versions)        │    │
│  │  • messaging  (conversations, messages, delivery)       │    │
│  │  • audit      (events, compliance logs)                 │    │
│  │                                                           │    │
│  │  Volume: sc_pgdata (persistent)                         │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │           Redis 7 (:16379)                               │    │
│  │  • Session cache                                         │    │
│  │  • Rate limit counters                                   │    │
│  │  • JWKS cache (30min TTL)                               │    │
│  │  Volume: sc_redis (AOF persistence)                     │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │           File Storage (Volume: sc_files)                │    │
│  │  /data/files/tenant_{id}/                               │    │
│  │    • Encrypted at rest                                   │    │
│  │    • Path: /data/files/{tenant_id}/{file_id}.enc        │    │
│  └──────────────────────────────────────────────────────────┘    │
└────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    Observability Layer                              │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐       │
│  │  Prometheus    │  │   Grafana      │  │  ELK Stack     │       │
│  │  (Metrics)     │  │  (Dashboards)  │  │  (Logs)        │       │
│  │  :9090         │  │  (Future)      │  │  (Future)      │       │
│  └────────────────┘  └────────────────┘  └────────────────┘       │
└─────────────────────────────────────────────────────────────────────┘
```

### Request Flow Diagram

```
┌──────────────┐
│  Qt Client   │
└──────┬───────┘
       │
       │ 1. POST /auth/login
       │    Body: {email, password}
       │
       ▼
┌──────────────────────────────────────────────┐
│           Gateway (:8080)                    │
│  ┌────────────────────────────────────┐     │
│  │ 1.1 TLS Termination                │     │
│  │ 1.2 Generate Request ID            │     │
│  │ 1.3 Route: /auth/* → auth-service  │     │
│  │ 1.4 Rate Limit Check (Global/IP)   │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 2. Proxy to auth-service:8001
       │
       ▼
┌──────────────────────────────────────────────┐
│        Auth Service (:8001)                  │
│  ┌────────────────────────────────────┐     │
│  │ 2.1 Validate credentials           │     │
│  │ 2.2 Query: auth.users              │     │
│  │ 2.3 Verify password hash           │     │
│  │ 2.4 Generate JWT (access + refresh)│     │
│  │ 2.5 Update last_login_timestamp    │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 3. Return JWT tokens
       │
       ▼
┌──────────────────────────────────────────────┐
│           Gateway (:8080)                    │
│  ┌────────────────────────────────────┐     │
│  │ 3.1 Log audit event                │     │
│  │ 3.2 Update metrics                 │     │
│  │     gateway_requests_total++       │     │
│  │     gateway_upstream_requests++    │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 4. Response: {access_token, refresh_token, expires_in}
       │
       ▼
┌──────────────┐
│  Qt Client   │ → Store JWT in memory
└──────────────┘
```

### Authenticated Request Flow

```
┌──────────────┐
│  Qt Client   │
└──────┬───────┘
       │
       │ 1. POST /files/upload
       │    Headers: Authorization: Bearer <JWT>
       │    Body: multipart/form-data
       │
       ▼
┌──────────────────────────────────────────────┐
│           Gateway (:8080)                    │
│  ┌────────────────────────────────────┐     │
│  │ 1.1 TLS Termination                │     │
│  │ 1.2 Generate Request/Correlation ID│     │
│  │ 1.3 JWT Validation                 │     │
│  │     • Extract token from header    │     │
│  │     • Verify signature (JWKS cache)│     │
│  │     • Check expiration             │     │
│  │     • Extract claims (user_id,     │     │
│  │       tenant_id, roles)            │     │
│  │ 1.4 Authorization Filter (RBAC)    │     │
│  │     • Check role: "user" required  │     │
│  │     • Verify tenant_id match       │     │
│  │ 1.5 Rate Limit (User quota)        │     │
│  │ 1.6 Route: /files/* → files-service│    │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 2. Proxy to files-service:8003
       │    Headers: X-User-ID, X-Tenant-ID, X-Request-ID
       │
       ▼
┌──────────────────────────────────────────────┐
│        Files Service (:8003)                 │
│  ┌────────────────────────────────────┐     │
│  │ 2.1 Extract user context from headers │  │
│  │ 2.2 Validate file size/type        │     │
│  │ 2.3 Generate file_id (UUID)        │     │
│  │ 2.4 Encrypt file (AES-256-GCM)     │     │
│  │ 2.5 Write to disk:                 │     │
│  │     /data/files/{tenant}/{file}.enc│     │
│  │ 2.6 Insert metadata:               │     │
│  │     files.documents table          │     │
│  │ 2.7 Emit audit event (async)       │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 3. Return file metadata
       │
       ▼
┌──────────────────────────────────────────────┐
│           Gateway (:8080)                    │
│  ┌────────────────────────────────────┐     │
│  │ 3.1 Propagate response             │     │
│  │ 3.2 Update metrics                 │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 4. Response: {file_id, size, upload_timestamp}
       │
       ▼
┌──────────────┐
│  Qt Client   │
└──────────────┘
```

### WebSocket Connection Flow

```
┌──────────────┐
│  Qt Client   │
└──────┬───────┘
       │
       │ 1. WebSocket Upgrade Request
       │    GET /ws/messages
       │    Upgrade: websocket
       │    Authorization: Bearer <JWT>
       │
       ▼
┌──────────────────────────────────────────────┐
│           Gateway (:8080)                    │
│  ┌────────────────────────────────────┐     │
│  │ 1.1 Detect WebSocket upgrade       │     │
│  │ 1.2 Validate JWT                   │     │
│  │ 1.3 Route: /ws/* → messaging       │     │
│  │ 1.4 Establish bidirectional proxy  │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 2. WebSocket connection established
       │
       ▼
┌──────────────────────────────────────────────┐
│      Messaging Service (:8004)               │
│  ┌────────────────────────────────────┐     │
│  │ 2.1 Accept WebSocket connection    │     │
│  │ 2.2 Subscribe to user's channels   │     │
│  │ 2.3 Handle real-time messages:     │     │
│  │     • Send message                 │     │
│  │     • Typing indicators            │     │
│  │     • Read receipts                │     │
│  │     • Delivery status              │     │
│  └────────────────────────────────────┘     │
└──────┬───────────────────────────────────────┘
       │
       │ 3. Bidirectional message flow
       │
       ▼
┌──────────────┐
│  Qt Client   │ → Real-time chat UI updates
└──────────────┘
```

---

## Component Specifications

### 1. Gateway Service

**Technology:** C++20, Boost.Beast, OpenSSL, jwt-cpp  
**Port:** 8080 (HTTP), 8443 (HTTPS), 9090 (Metrics)  
**Status:** 90% complete (Steps 1-9 implemented)

#### Responsibilities

- **TLS Termination**: Decrypt HTTPS traffic, offload encryption from microservices
- **Authentication**: Validate JWT tokens using cached JWKS
- **Authorization**: Enforce RBAC policies based on JWT claims
- **Routing**: Dynamic regex-based routing to microservices
- **Rate Limiting**: Multi-strategy (global, per-IP, per-user, per-endpoint)
- **WebSocket Proxy**: Bidirectional proxying for real-time messaging
- **Observability**: Prometheus metrics export, structured logging

#### Configuration

```yaml
# gateway/config/gateway.dev.yaml (example)
server:
  host: "0.0.0.0"
  port: 8080
  threads: 4

tls:
  enabled: true
  cert_file: "/certs/server.crt"
  key_file: "/certs/server.key"
  ca_file: "/certs/ca.crt"
  verify_client: false

security:
  jwt:
    secret: "change-me-in-production"
    issuer: "securecloud"
    audience: "securecloud-api"
    jwks_cache_ttl_sec: 1800  # 30 minutes
  
  auth_cache:
    max_entries: 10000
    ttl_seconds: 300  # 5 minutes

rate_limits:
  global:
    requests_per_second: 1000
  per_ip:
    requests_per_second: 10
  per_user:
    requests_per_minute: 600

routes:
  - path: "^/auth/.*"
    upstream: "auth-service"
    methods: ["POST", "GET"]
    
  - path: "^/files/.*"
    upstream: "files-service"
    methods: ["POST", "GET", "DELETE"]
    auth_required: true
    
  - path: "^/ws/.*"
    upstream: "messaging-service"
    websocket: true
    auth_required: true

upstreams:
  auth-service:
    type: "http"
    url: "http://auth-service:8001"
    
  files-service:
    type: "http"
    url: "http://files-service:8003"
    
  messaging-service:
    type: "http"
    url: "http://messaging-service:8004"
```

#### Key Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `gateway_requests_total` | Counter | Total HTTP requests (by method, route, status) |
| `gateway_request_duration_seconds` | Histogram | Request latency distribution |
| `gateway_routing_matches_total` | Counter | Route matching success/failure |
| `gateway_upstream_requests_total` | Counter | Per-upstream request count |
| `gateway_jwt_validations_total` | Counter | JWT validation results |
| `gateway_rate_limit_hits_total` | Counter | Rate limiter triggered events |

---

### 2. Auth Service

**Technology:** C++20, Crow (HTTP framework), bcrypt, jwt-cpp  
**Port:** 8001  
**Database Schema:** `auth.*`  
**Status:** 15% complete (login endpoint only)

#### Responsibilities

- **User Authentication**: Credential validation, password hashing (bcrypt)
- **JWT Issuance**: Generate access/refresh tokens
- **Session Management**: Track active sessions, handle logout
- **User Registration**: Account creation, email verification
- **Password Management**: Reset flows, password change
- **MFA**: Multi-factor authentication setup/verification
- **Tenant Management**: Multi-tenancy isolation

#### API Endpoints

| Endpoint | Method | Status | Description |
|----------|--------|--------|-------------|
| `/auth/login` | POST |  Implemented | Login with credentials |
| `/auth/logout` | POST |  Not implemented | Revoke tokens |
| `/auth/refresh` | POST |  Not implemented | Refresh access token |
| `/auth/register` | POST |  Not implemented | Create new user |
| `/auth/verify-email` | POST |  Not implemented | Confirm email |
| `/auth/reset-password` | POST |  Not implemented | Password reset |
| `/auth/me` | GET |  Not implemented | Get current user |
| `/auth/mfa/setup` | POST |  Not implemented | Setup TOTP MFA |
| `/auth/mfa/verify` | POST |  Not implemented | Verify MFA code |

#### Database Schema (auth.*)

```sql
-- auth.tenants
CREATE TABLE auth.tenants (
    tenant_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    domain VARCHAR(255) UNIQUE,
    created_at TIMESTAMP DEFAULT NOW(),
    status VARCHAR(20) DEFAULT 'active'
);

-- auth.users
CREATE TABLE auth.users (
    user_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id UUID REFERENCES auth.tenants(tenant_id),
    email VARCHAR(255) UNIQUE NOT NULL,
    pass_hash VARCHAR(255) NOT NULL,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    role VARCHAR(50) DEFAULT 'user',
    mfa_enabled BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT NOW(),
    last_login_timestamp TIMESTAMP
);

-- auth.sessions
CREATE TABLE auth.sessions (
    session_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(user_id),
    refresh_token_hash VARCHAR(255),
    ip_address INET,
    user_agent TEXT,
    created_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP,
    revoked BOOLEAN DEFAULT FALSE
);
```

---

### 3. Files Service

**Technology:** C++20, Crow, OpenSSL (AES-256-GCM)  
**Port:** 8003  
**Database Schema:** `files.*`  
**Storage:** `/data/files/` (Docker volume `sc_files`)  
**Status:** 0% implemented (schema complete)

#### Responsibilities

- **File Upload**: Chunked upload support, multipart/form-data
- **File Download**: Streaming download, resume support
- **Encryption**: AES-256-GCM encryption at rest
- **Access Control**: Per-file permissions, sharing
- **Versioning**: Track file versions, restore previous versions
- **Metadata Management**: File size, MIME type, checksums

#### API Endpoints (Planned)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/files/upload` | POST | Upload file (chunked) |
| `/files/{id}` | GET | Download file |
| `/files/{id}` | DELETE | Delete file |
| `/files` | GET | List user's files |
| `/files/{id}/share` | POST | Share file with users |
| `/files/{id}/versions` | GET | List file versions |

#### Database Schema (files.*)

```sql
-- files.documents
CREATE TABLE files.documents (
    file_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id UUID NOT NULL,
    owner_id UUID NOT NULL,
    filename VARCHAR(255) NOT NULL,
    file_path VARCHAR(500) NOT NULL,
    size_bytes BIGINT NOT NULL,
    mime_type VARCHAR(100),
    encryption_key_id UUID,
    checksum_sha256 VARCHAR(64),
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- files.permissions
CREATE TABLE files.permissions (
    permission_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    file_id UUID REFERENCES files.documents(file_id),
    user_id UUID NOT NULL,
    permission_type VARCHAR(20) NOT NULL, -- read, write, admin
    granted_at TIMESTAMP DEFAULT NOW()
);
```

---

### 4. Messaging Service

**Technology:** C++20, Crow, Boost.Beast (WebSockets)  
**Port:** 8004  
**Database Schema:** `messaging.*`  
**Status:** 0% implemented (schema complete)

#### Responsibilities

- **Real-time Messaging**: WebSocket connections for instant delivery
- **Conversations**: 1-on-1 and group chats
- **Message Encryption**: End-to-end encryption support
- **Delivery Tracking**: Read receipts, delivery status
- **Typing Indicators**: Real-time user activity
- **Message History**: Paginated message retrieval

#### API Endpoints (Planned)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/ws/messages` | WebSocket | Real-time message stream |
| `/conversations` | GET | List user's conversations |
| `/conversations` | POST | Create conversation |
| `/conversations/{id}/messages` | GET | Get message history |
| `/conversations/{id}/messages` | POST | Send message |
| `/messages/{id}/read` | POST | Mark as read |

#### Database Schema (messaging.*)

```sql
-- messaging.conversations
CREATE TABLE messaging.conversations (
    conversation_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id UUID NOT NULL,
    name VARCHAR(255),
    type VARCHAR(20) NOT NULL, -- direct, group
    created_at TIMESTAMP DEFAULT NOW()
);

-- messaging.messages
CREATE TABLE messaging.messages (
    message_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID REFERENCES messaging.conversations(conversation_id),
    sender_id UUID NOT NULL,
    content_encrypted TEXT NOT NULL,
    encryption_key_id UUID,
    sent_at TIMESTAMP DEFAULT NOW()
);

-- messaging.delivery_status
CREATE TABLE messaging.delivery_status (
    delivery_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    message_id UUID REFERENCES messaging.messages(message_id),
    recipient_id UUID NOT NULL,
    delivered_at TIMESTAMP,
    read_at TIMESTAMP
);
```

---

### 5. Audit Service

**Technology:** C++20, Crow  
**Port:** 8002  
**Database Schema:** `audit.*`  
**Status:** 0% implemented (schema complete)

#### Responsibilities

- **Event Logging**: Record all security-sensitive operations
- **Compliance**: GDPR, HIPAA audit trail support
- **Query API**: Search audit logs by user, action, date range
- **Retention**: Configurable log retention policies
- **Alerting**: Real-time alerts on suspicious activity

#### Database Schema (audit.*)

```sql
-- audit.events
CREATE TABLE audit.events (
    event_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    tenant_id UUID NOT NULL,
    user_id UUID,
    action VARCHAR(100) NOT NULL,
    resource_type VARCHAR(50),
    resource_id UUID,
    ip_address INET,
    user_agent TEXT,
    request_id UUID,
    event_timestamp TIMESTAMP DEFAULT NOW(),
    metadata JSONB
);
```

---

## API Contracts

### Authentication Flow

#### 1. Login Request

```http
POST /auth/login HTTP/1.1
Host: gateway:8080
Content-Type: application/json

{
  "email": "user@example.com",
  "password": "secure_password123"
}
```

**Response (200 OK):**

```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "refresh_token_value",
  "expires_in": 3600,
  "token_type": "Bearer",
  "user": {
    "user_id": "123e4567-e89b-12d3-a456-426614174000",
    "email": "user@example.com",
    "role": "user",
    "tenant_id": "tenant_uuid"
  }
}
```

**JWT Payload:**

```json
{
  "sub": "123e4567-e89b-12d3-a456-426614174000",
  "email": "user@example.com",
  "role": "user",
  "tenant_id": "tenant_uuid",
  "iss": "securecloud",
  "aud": "securecloud-api",
  "exp": 1704587345,
  "iat": 1704583745
}
```

#### 2. Authenticated Request

```http
GET /files HTTP/1.1
Host: gateway:8080
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

**Gateway adds context headers to upstream:**

```http
GET /files HTTP/1.1
Host: files-service:8003
X-User-ID: 123e4567-e89b-12d3-a456-426614174000
X-Tenant-ID: tenant_uuid
X-User-Role: user
X-Request-ID: req_9876543210
X-Correlation-ID: corr_abcdef123456
```

---

## Data Flow & Communication

### Service Communication Patterns

1. **Client → Gateway**: HTTPS (TLS 1.2+)
2. **Gateway → Microservices**: HTTP (internal Docker network)
3. **Microservices → PostgreSQL**: TCP/5432 (SSL optional)
4. **Microservices → Redis**: TCP/6379 (AUTH required)

### Message Flow Example: File Upload

```
1. Client → Gateway
   POST /files/upload
   Headers: Authorization, Content-Type: multipart/form-data
   Body: file binary data

2. Gateway Processing
   - Verify JWT signature
   - Extract claims: user_id, tenant_id
   - Check RBAC: user has "upload" permission
   - Check rate limit: user quota available
   - Forward to files-service with context headers

3. Files Service Processing
   - Validate file size (max 100MB)
   - Validate MIME type (allow-list)
   - Generate file_id (UUID)
   - Encrypt file (AES-256-GCM)
   - Write to: /data/files/{tenant_id}/{file_id}.enc
   - Insert metadata to files.documents
   - Emit audit event to audit-service (async)

4. Response Path
   Files Service → Gateway → Client
   Response: {file_id, size, checksum}

5. Async Operations
   - Audit Service logs file upload event
   - Metrics updated: files_uploaded_total++
```

### Error Handling

```
Gateway Error Responses:
- 401 Unauthorized: Missing/invalid JWT
- 403 Forbidden: RBAC check failed
- 429 Too Many Requests: Rate limit exceeded
- 502 Bad Gateway: Upstream service unavailable
- 504 Gateway Timeout: Upstream timeout

Service Error Responses:
- 400 Bad Request: Invalid request payload
- 404 Not Found: Resource doesn't exist
- 409 Conflict: Duplicate resource
- 500 Internal Server Error: Service fault
```

---

## Technology Stack

### Backend Services

| Component | Technology | Version | Purpose |
|-----------|-----------|---------|---------|
| Gateway | C++20, Boost.Beast | - | High-performance HTTP/WebSocket gateway |
| Microservices | C++20, Crow | - | RESTful API services |
| Database | PostgreSQL | 16 | Persistent storage (4 schemas) |
| Cache | Redis | 7 | Session cache, rate limiting |
| Encryption | OpenSSL | 3.x | TLS, AES-256-GCM, JWT |
| Logging | spdlog | - | Structured logging |
| Metrics | Prometheus | - | Observability |

### Client

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Desktop UI | Qt 6 | Native cross-platform UI |
| HTTP Client | Qt Network | REST API consumption |
| WebSocket | Qt WebSockets | Real-time messaging |

### Infrastructure

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Containerization | Docker, Docker Compose | Service orchestration |
| Orchestration (Future) | Kubernetes | Production deployment |
| CI/CD (Future) | GitHub Actions | Automated builds/tests |
| Monitoring (Future) | Grafana | Metrics dashboards |

### Development Tools

- **Build System**: CMake 3.27+, vcpkg
- **Compiler**: MSVC 2022, GCC 11+, Clang 14+
- **Testing**: Catch2, GoogleTest
- **Linting**: clang-format, clang-tidy
- **Documentation**: Markdown, Doxygen (future)

---

## Deployment Architecture

### Docker Compose (Development/Staging)

```yaml
# Simplified structure
services:
  gateway:       # C++ gateway
  auth-service:  # C++ microservice
  files-service: # C++ microservice
  messaging-service: # C++ microservice
  audit-service: # C++ microservice
  postgres:      # PostgreSQL 16
  redis:         # Redis 7
  prometheus:    # Metrics collection

volumes:
  sc_pgdata:     # PostgreSQL data
  sc_redis:      # Redis persistence
  sc_files:      # Uploaded files
  sc_logs:       # Application logs

networks:
  securecloud-net:  # Internal bridge network
```

### Network Topology

```
┌─────────────────────────────────────────────────────────┐
│  Docker Host (Windows/Linux)                            │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │  Docker Network: securecloud-net (bridge)      │    │
│  │                                                 │    │
│  │  Gateway:       172.18.0.2:8080                │    │
│  │  Auth Service:  172.18.0.3:8001                │    │
│  │  Files Service: 172.18.0.4:8003                │    │
│  │  PostgreSQL:    172.18.0.5:5432                │    │
│  │  Redis:         172.18.0.6:6379                │    │
│  └────────────────────────────────────────────────┘    │
│                                                          │
│  Port Mapping (localhost):                              │
│  - 8080  → Gateway HTTP                                 │
│  - 8443  → Gateway HTTPS                                │
│  - 9090  → Prometheus Metrics                           │
│  - 15432 → PostgreSQL (debug only)                      │
│  - 16379 → Redis (debug only)                           │
└─────────────────────────────────────────────────────────┘
```

### Kubernetes Architecture (Future)

```yaml
Namespaces:
  - securecloud-prod
  - securecloud-staging

Deployments:
  - gateway (replicas: 3)
  - auth-service (replicas: 2)
  - files-service (replicas: 2)
  - messaging-service (replicas: 2)
  - audit-service (replicas: 1)

StatefulSets:
  - postgresql (replicas: 3, HA cluster)
  - redis (replicas: 3, Sentinel)

Services:
  - gateway (LoadBalancer)
  - Internal services (ClusterIP)

Ingress:
  - HTTPS termination
  - cert-manager for Let's Encrypt

Persistent Volumes:
  - PostgreSQL data (ReadWriteOnce)
  - File storage (ReadWriteMany, NFS/EFS)

ConfigMaps/Secrets:
  - Service configuration
  - TLS certificates
  - Database credentials
```

---

## Scalability & Performance

### Performance Targets

| Metric | Target | Current Status |
|--------|--------|----------------|
| Gateway RPS | 10,000 req/s | TBD (load testing pending) |
| Gateway p99 Latency | < 100ms | TBD |
| Database Connections | 100 per service | TBD |
| WebSocket Connections | 10,000 concurrent | TBD |
| File Upload Speed | 50 MB/s | TBD |

### Scalability Strategy

1. **Horizontal Scaling**
   - Gateway: Scale to N replicas (stateless)
   - Microservices: Scale independently based on load
   - Database: Read replicas for query offloading

2. **Caching Strategy**
   - JWT validation: JWKS cache (30min TTL)
   - Auth cache: User session cache (5min TTL)
   - Redis: Distributed cache for rate limiting

3. **Database Optimization**
   - Connection pooling (per service)
   - Read replicas for query-heavy services
   - Partitioning: `audit.events` by date range

4. **Load Balancing**
   - Gateway: Layer 7 load balancing (Kubernetes Ingress)
   - Internal services: Round-robin DNS (kube-dns)

### Bottlenecks & Mitigations

| Bottleneck | Impact | Mitigation |
|------------|--------|------------|
| Database connections | Service startup delays | Connection pooling, prepared statements |
| File I/O | Upload/download slowness | Chunked transfers, streaming |
| JWT validation | High CPU on gateway | JWKS caching, async validation |
| WebSocket connections | Memory exhaustion | Connection limits, heartbeat timeouts |

---

## Future Enhancements

### Phase 2 Features

- [ ] **Kubernetes Deployment**: Helm charts, auto-scaling
- [ ] **Advanced Monitoring**: Grafana dashboards, alerting
- [ ] **End-to-End Encryption**: Client-side encryption for files/messages
- [ ] **Advanced RBAC**: Fine-grained permissions, attribute-based access control
- [ ] **API Gateway Enhancements**: GraphQL support, gRPC proxying

### Phase 3 Features

- [ ] **Mobile Clients**: iOS/Android apps
- [ ] **Web Client**: React/Vue.js frontend
- [ ] **Advanced Audit**: Machine learning for anomaly detection
- [ ] **Compliance**: SOC 2, ISO 27001 certification support
- [ ] **Multi-Region**: Geographic distribution, data residency

---

## References

- [Gateway README](../gateway/README.md) - Detailed gateway implementation
- [Data Flow & Persistence](data-flow-persistence.md) - Data architecture details
- [Security Documentation](security.md) - Security architecture
- [Docker Compose](../docker-compose.yml) - Complete service definitions
- [Database Migrations](../db/migrations/) - Schema definitions

---

**Document Status:** Living document - update as architecture evolves  
**Next Review:** After Gateway Step 10 completion  
**Maintainers:** SecureCloud Development Team
