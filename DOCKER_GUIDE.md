# SecureCloud Docker Compose Guide

Complete guide covering the build-to-deploy process for all SecureCloud services.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Build Process](#build-process)
3. [Docker Build Phase](#docker-build-phase)
4. [Deployment](#deployment)
5. [Best Practices](#best-practices)
6. [Commands Reference](#commands-reference)
7. [Troubleshooting](#troubleshooting)

---

## Architecture Overview

### Complete Build-to-Deploy Flow

```
+----------------------------------------------------------+
|                    DEVELOPMENT PHASE                      |
+----------------------------------------------------------+
|  Local Build (MSYS2 UCRT64)                              |
|  cmake -B build/dev --preset dev                          |
|  cmake --build build/dev                                  |
+----------------------------------------------------------+
                           |
                           v
+----------------------------------------------------------+
|                    DOCKER BUILD PHASE                     |
+----------------------------------------------------------+
|  Multi-stage Dockerfiles (ops/docker/*.Dockerfile)       |
|  - Stage 1: Build (gcc/cmake in container)               |
|  - Stage 2: Runtime (minimal Ubuntu image)               |
+----------------------------------------------------------+
                           |
                           v
+----------------------------------------------------------+
|                    DEPLOYMENT PHASE                       |
+----------------------------------------------------------+
|  docker compose.yml                                       |
|  - Infrastructure: PostgreSQL, Redis                      |
|  - Migrations: Flyway (per schema)                        |
|  - Services: Gateway, Auth, Files, Messaging, Audit       |
|  - Monitoring: Prometheus, Grafana, Adminer              |
+----------------------------------------------------------+
```

### Service Architecture

```
+-------------------------------------------------------------+
|                 DOCKER NETWORK (securecloud-net)            |
+-------------------------------------------------------------+
|                                                             |
|    +-------------------+                                    |
|    |      Gateway      | :8080 (HTTP) / :8443 (HTTPS)      |
|    +--------+----------+                                    |
|             |                                               |
|       +-----+-----+-----+-----+-----+                      |
|       v           v     v     v     v                      |
|   +-------+ +-------+ +-----+ +-------+ +--------+         |
|   | Auth  | | Files | | Msg | | Audit | | Deploy |         |
|   | :8001 | | :8003 | |:8004| | :8002 | | :8006  |         |
|   +---+---+ +---+---+ +--+--+ +---+---+ +--------+         |
|       |         |        |        |                        |
|       +----+----+--------+--------+                        |
|            |                                               |
|      +-----v------+    +--------+                          |
|      | PostgreSQL |    | Redis  |                          |
|      |   :5432    |    | :6379  |                          |
|      +------------+    +--------+                          |
|                                                             |
+-------------------------------------------------------------+
```

---

## Build Process

### Phase 1: Local Development Build

#### Step 1: Configure the Project

```bash
# From project root (UCRT64 terminal)
cmake -B build/dev --preset dev
```

**What happens:**
1. CMake reads `CMakePresets.json` with preset `dev`
2. Creates build directory at `build/dev/` (pattern: `build/${presetName}`)
3. Configures Ninja as generator, sets Debug mode
4. Detects MSYS2 UCRT64 toolchain

#### Step 2: Build All Services

```bash
cmake --build build/dev -j 16
```

**Build targets created:**

| Target | Output | Location |
|--------|--------|----------|
| `gateway` | `gateway.exe` | `build/dev/gateway/` |
| `auth-service` | `auth-service.exe` | `build/dev/services/auth-service/` |
| `MSF_Login` | `MSF_Login.exe` | `build/dev/client/qt-app/` |

> **Note:** Files, Messaging, and Audit services are stub placeholders - full implementation pending.

#### Step 3: Start Database

```bash
cmake --build build/dev --target db-up
cmake --build build/dev --target db-migrate
```

**What happens:**
1. `db-up`: Starts PostgreSQL container on port 15432
2. `db-migrate`: Runs Flyway migrations for all 4 schemas:
   - `auth` schema (users, roles, tokens)
   - `files` schema (file metadata)
   - `messaging` schema (messages, channels)
   - `audit` schema (audit logs)

---

## Docker Build Phase

### Multi-Stage Dockerfile Pattern

Each service uses a **multi-stage build** for smaller, secure images:

```dockerfile
# ========== STAGE 1: BUILD ==========
FROM gcc:15 AS build

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake libboost-all-dev libssl-dev ...

# Copy source and build
COPY services/auth-service ./services/auth-service
WORKDIR /src/services/auth-service
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# ========== STAGE 2: RUNTIME ==========
FROM ubuntu:24.04

# Install only runtime dependencies (no compilers)
RUN apt-get update && apt-get install -y \
    libssl3 libboost-system1.83.0 ca-certificates

# Copy only the binary (not source code)
COPY --from=build /src/services/auth-service/build/auth-service /app/
COPY services/auth-service/config /app/config

ENTRYPOINT ["/app/auth-service"]
```

**Benefits:**
- Build stage: ~2GB (with gcc, cmake, dev headers)
- Runtime stage: ~100MB (minimal dependencies)
- Source code NOT in final image (security)

### Service Dockerfile Locations

| Service | Dockerfile |
|---------|------------|
| Gateway | `ops/docker/gateway.Dockerfile` |
| Auth | `ops/docker/auth-service.Dockerfile` |
| Files | `ops/docker/files-service.Dockerfile` |
| Messaging | `ops/docker/messaging-service.Dockerfile` |
| Audit | `ops/docker/audit-service.Dockerfile` |

---

## Deployment

### Startup Order (Dependency Chain)

```
1. Infrastructure
   +-> postgres (healthcheck: pg_isready)
   +-> redis (healthcheck: redis-cli ping)

2. Database Migrations (run once, then exit)
   +-> flyway-auth (depends: postgres healthy)
   +-> flyway-files
   +-> flyway-messaging  
   +-> flyway-audit

3. Backend Services (wait for migrations)
   +-> auth-service (depends: postgres, redis, flyway-auth)
   +-> audit-service (depends: postgres, flyway-audit)
   +-> files-service (depends: postgres, flyway-files)
   +-> messaging-service (depends: postgres, redis, flyway-messaging)

4. Gateway (wait for all services)
   +-> gateway (depends: auth, audit, files, messaging healthy)

5. Monitoring (optional profile)
   +-> prometheus
   +-> grafana
```

### Deploy Commands

```bash
# Option 1: Via CMake targets (recommended)
cmake --build build/dev --target docker-up

# Option 2: Direct Docker Compose
docker compose up -d

# Option 3: With monitoring stack
docker compose --profile monitoring up -d
```

### Quick Start

```bash
# 1. Copy environment template
cp .env.example .env

# 2. Edit .env with your values (especially passwords and secrets)
nano .env

# 3. Start all services
docker compose up -d

# 4. Check status
docker compose ps

# 5. View logs
docker compose logs -f gateway
```

## Service Architecture

```
┌─────────────────────────────────────────┐
│          Gateway (8080/8443)            │
├─────────────────────────────────────────┤
│  ┌──────────────┬──────────────────┐   │
│  │ Auth Service │  Audit Service   │   │
│  │    :8001     │      :8002       │   │
│  ├──────────────┼──────────────────┤   │
│  │ Files Service│ Messaging Service│   │
│  │    :8003     │  :8004 / :8005   │   │
│  ├──────────────┴──────────────────┤   │
│  │      Deploy Service :8006       │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
         │                    │
    ┌────────┐         ┌──────────┐
    │ Postgres│         │   Redis   │
    │  :15432 │         │   :16379  │
    └─────────┘         └───────────┘
```

## Available Commands

### CMake Integration (Recommended)

```bash
# Start only PostgreSQL (for local development)
cmake --build build/dev --target db-up

# Run all migrations
cmake --build build/dev --target db-migrate

# Start all Docker services
cmake --build build/dev --target docker-up

# Stop all services
cmake --build build/dev --target docker-down

# View status
cmake --build build/dev --target docker-status

# View logs
cmake --build build/dev --target docker-logs
```

### Direct Docker Compose Commands

#### Start Services
```bash
# All services
docker compose up -d

# Specific service
docker compose up -d gateway auth-service

# With monitoring (Prometheus + Grafana)
docker compose --profile monitoring up -d
```

### Stop Services
```bash
# Stop all
docker compose down

# Stop and remove volumes ( data loss)
docker compose down -v
```

### View Logs
```bash
# All services
docker compose logs -f

# Specific service
docker compose logs -f gateway

# Last 100 lines
docker compose logs --tail=100 auth-service
```

### Rebuild Services
```bash
# Rebuild all
docker compose build

# Rebuild specific service
docker compose build gateway

# Rebuild and restart
docker compose up -d --build gateway
```

### Database Management
```bash
# Run migrations via CMake
cmake --build build/dev --target db-migrate

# Or run migrations directly
docker compose up flyway-auth flyway-audit flyway-files flyway-messaging

# Access PostgreSQL
docker compose exec postgres psql -U scuser -d securecloud

# Backup database
docker compose exec postgres pg_dump -U scuser securecloud > backup.sql

# Restore database
docker compose exec -T postgres psql -U scuser securecloud < backup.sql
```

### Service Health Check
```bash
# Check all services
docker compose ps

# Check specific service health
curl http://localhost:8080/health  # Gateway
curl http://localhost:8001/health  # Auth Service
curl http://localhost:8002/health  # Audit Service
```

## Port Mapping

| Service | Port | Description |
|---------|------|-------------|
| Gateway HTTP | 8080 | Main API Gateway |
| Gateway HTTPS | 8443 | TLS Gateway |
| Auth Service | 8001 | Authentication |
| Audit Service | 8002 | Audit Logging |
| Files Service | 8003 | File Management |
| Messaging (HTTP) | 8004 | Messaging API |
| Messaging (WebSocket) | 8005 | Real-time Chat |
| Deploy Service | 8006 | Deployment |
| PostgreSQL | 15432 | Database |
| Redis | 16379 | Cache/Sessions |
| Adminer | 9090 | DB Admin UI |
| Prometheus | 9091 | Metrics |
| Grafana | 3000 | Dashboards |

## Best Practices

### 1. Environment Configuration

```bash
# Use .env files for environment-specific config
config/env/
+-- dev/.env          # Development settings
+-- staging/.env      # Staging settings  
+-- prod/.env         # Production settings (NOT in git)
```

**Key variables to change in production:**
```env
# Database - MUST change
DB_PASS=strong_random_password_here

# JWT - CRITICAL (min 32 chars)
JWT_SECRET=your_production_secret_key_minimum_32_characters

# TLS - Enable in production
ENABLE_TLS=true
```

### 2. Health Checks

All services implement `/health` endpoint:

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8001/health"]
  interval: 10s
  timeout: 5s
  retries: 5
  start_period: 30s
```

### 3. Volume Strategy

```yaml
volumes:
  sc_pgdata:      # Database persistence (CRITICAL)
  sc_redis:       # Session cache
  sc_files:       # Uploaded files
  sc_logs:        # Application logs
```

**Data survives:**
- Container restarts
- `docker compose down`

**Data LOST with:**
- `docker compose down -v` (removes volumes)

### 4. Network Isolation

```yaml
networks:
  securecloud-net:
    driver: bridge
```

- Services communicate via internal DNS (`auth-service:8001`)
- Only Gateway exposed externally (ports 8080, 8443)
- Database NOT exposed in production

### 5. Build Caching

Order COPY commands by change frequency in Dockerfiles:

```dockerfile
# Rarely changes - cached
COPY CMakeLists.txt .
RUN cmake -B build

# Changes frequently - rebuilt
COPY src/ ./src/
RUN cmake --build build
```

---

## Environment Variables

### Required
- `DB_PASS` - PostgreSQL password
- `REDIS_PASSWORD` - Redis password  
- `JWT_SECRET` - JWT signing key (min 32 chars)

### Optional
- `LOG_LEVEL` - Logging level (debug, info, warn, error)
- `ENABLE_TLS` - Enable HTTPS on gateway
- All port overrides (see .env.example)

## Monitoring

Start with monitoring profile:
```bash
docker compose --profile monitoring up -d
```

Access:
- Prometheus: http://localhost:9091
- Grafana: http://localhost:3000 (admin/admin)

## Development vs Production

### Development Workflow

```bash
# 1. Initial setup (once)
cmake -B build/dev --preset dev

# 2. Start database
cmake --build build/dev --target db-up
cmake --build build/dev --target db-migrate

# 3. Build services
cmake --build build/dev -j 16

# 4. Run locally for testing
cmake --build build/dev --target run-gateway

# 5. Test with Docker
cmake --build build/dev --target docker-up

# 6. View logs
cmake --build build/dev --target docker-logs

# 7. Stop everything
cmake --build build/dev --target docker-down
```

### Production Deployment

```bash
# 1. Build optimized images
docker compose build --no-cache

# 2. Push to registry (if using remote registry)
docker compose push

# 3. Deploy (on production server)
docker compose -f docker compose.yml up -d

# 4. Verify migrations ran
docker compose logs flyway-auth

# 5. Verify all services healthy
docker compose ps
curl http://localhost:8080/health
```

### Production Checklist

- [ ] Change all default passwords in `.env`
- [ ] Set `JWT_SECRET` to a secure random value (min 32 chars)
- [ ] Enable TLS (`ENABLE_TLS=true`)
- [ ] Configure proper TLS certificates
- [ ] Set `LOG_LEVEL=info` (not debug)
- [ ] Configure backup strategy for PostgreSQL
- [ ] Set up monitoring (Prometheus/Grafana)
- [ ] Configure firewall rules

## Troubleshooting

### Service won't start
```bash
# Check logs
docker compose logs service-name

# Check dependencies
docker compose ps

# Restart service
docker compose restart service-name
```

### Database connection issues
```bash
# Check postgres is running
docker compose ps postgres

# Test connection
docker compose exec postgres pg_isready -U scuser

# Check migrations
docker compose logs flyway-auth
```

### Network issues
```bash
# Recreate network
docker compose down
docker network prune
docker compose up -d
```

### Reset everything
```bash
#  This will delete all data
docker compose down -v
docker compose up -d
```

## CI/CD Integration

### GitHub Actions Example
```yaml
- name: Build and test
  run: |
    docker compose build
    docker compose up -d
    docker compose exec -T gateway ./tests/integration
    docker compose down
```

## Security Notes

1. **Change default passwords** in `.env`
2. **Use secrets management** in production (Docker Secrets, Vault)
3. **Enable TLS** with valid certificates
4. **Restrict network access** using firewall rules
5. **Regular updates** of base images

## Performance Tuning

### Resource Limits
Edit docker-compose.yml to add:
```yaml
services:
  gateway:
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
```

### Scaling
```bash
# Scale messaging service
docker compose up -d --scale messaging-service=3
```

## Backup Strategy

```bash
# Automated backup script
#!/bin/bash
DATE=$(date +%Y%m%d_%H%M%S)
docker compose exec postgres pg_dump -U scuser securecloud | gzip > backup_$DATE.sql.gz
docker compose exec redis redis-cli --rdb /data/dump.rdb
docker cp sc_redis:/data/dump.rdb redis_backup_$DATE.rdb
```

---

## Key Files Reference

| Purpose | File |
|---------|------|
| Build configuration | `CMakeLists.txt` |
| Build presets | `CMakePresets.json` |
| Full stack deployment | `docker-compose.yml` |
| Dev database only | `ops/compose/compose.dev.yml` |
| Service Dockerfiles | `ops/docker/*.Dockerfile` |
| Environment config | `config/env/dev/.env` |
| Database migrations | `db/migrations/{schema}/` |

---

## Related Documentation

- [README.md](README.md) - Project overview
- [QUICK_START_FR.md](QUICK_START_FR.md) - Quick start guide (French)
- [INSTALL_FR.md](INSTALL_FR.md) - Installation guide (French)
- [docs/architecture.md](docs/architecture.md) - System architecture
- [docs/security.md](docs/security.md) - Security documentation
