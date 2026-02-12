# SecureCloud

A secure cloud platform built with C++20, featuring a Gateway, Authentication, Files, Messaging, and Audit services, with a Qt-based desktop client.

## Quick Start

This README is the canonical overview; for quick developer steps see `docs/QUICK_START_FR.md`.

### Prerequisites (short)

- MSYS2 (UCRT64), Ninja, Docker Desktop, Qt 6.8+

### Minimal local setup

```powershell
# Configure
cmake -B build/dev --preset dev

# Start DB (once)
cmake --build build/dev --target db-up
cmake --build build/dev --target db-migrate

# Build everything (or target a single component)
cmake --build build/dev -j 16
cmake --build build/dev --target gateway

# Run a service (example)
cmake --build build/dev --target run-gateway
```

### Docker-first workflow

```powershell
# Compose files:
#   core: ops/compose/compose.core.yml
#   full overlay (planned/experimental services): ops/compose/compose.full.yml
#   desktop profile: qt-client container

# Build Docker images for the stack
cmake --build build/dev --target docker-build

# Start core stack (gateway + auth + postgres + redis)
cmake --build build/dev --target docker-up-core

# Start full stack (planned services behind "full" profile)
cmake --build build/dev --target docker-up-full

# Start core stack + Qt desktop client container ("desktop" profile)
cmake --build build/dev --target docker-up-desktop

# Start monitoring profile (Prometheus + Grafana)
cmake --build build/dev --target docker-up-monitoring
```

Direct Compose equivalent (recommended when debugging startup):

```powershell
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml up -d --build
```

Important:
- Use `--env-file config/env/dev/.env` to avoid silent fallback to defaults from an empty/missing root `.env`.
- `db-*` and `docker-*` CMake targets now use the same compose stack (`ops/compose/compose.core.yml` + `ops/compose/compose.full.yml`).

### Auth session smoke test (E2E)

```powershell
# Optional overrides:
#   $env:SMOKE_EMAIL="your-user@example.com"
#   $env:SMOKE_PASSWORD="your-password"
cmake --build build/dev --target smoke-auth-session
```

### Auth session E2E with GoogleTest (C++)

```powershell
# Optional overrides:
#   $env:SC_E2E_EMAIL="your-user@example.com"
#   $env:SC_E2E_PASSWORD="your-password"
#   $env:SC_E2E_GATEWAY_HOST="127.0.0.1"
#   $env:SC_E2E_GATEWAY_PORT="8443"
cmake --build build/dev --target auth_session_e2e_tests
ctest --test-dir build/dev -R auth_session_e2e_tests --output-on-failure
```

---

## Project Structure

```text
SecureCloud/
├── gateway/                # C++ API Gateway (Boost.Beast, TLS, JWT)
├── services/
│   ├── auth-service/       # Authentication & user management
│   ├── files-service/      # File storage & encryption
│   ├── messaging-service/  # Real-time messaging (WebSocket)
│   └── audit-service/      # Compliance & audit logging
├── client/qt-app/          # Qt6 Desktop Client
├── db/migrations/          # Flyway migrations (per schema)
├── ops/                    # Docker & Compose configs
├── config/env/             # Environment files (.env)
└── docs/                   # Architecture & API docs
```

---

## VS Code Tasks

Open workspace `securecloud.code-workspace`, then use `Ctrl+Shift+B` or `Ctrl+Shift+P` > "Tasks: Run Task".

| Task | Description |
|------|-------------|
| `Build All` | Compile all components (default) |
| `Build Gateway` | Compile Gateway only |
| `Build Auth Service` | Compile Auth Service only |
| `Build Qt Client` | Compile Qt Client only |
| `Start Database` | PostgreSQL + migrations |
| `Run Gateway` | Launch Gateway (:8443) |
| `Run Auth Service` | Launch Auth Service (:8001) |
| `Run Qt Client` | Launch Qt Client (GUI) |
| `Start Dev Environment` | DB + all services (one-click) |

---

## Default Credentials (Development)

| Service | Host | Port | User | Password |
|---------|------|------|------|----------|
| Gateway HTTPS | localhost | 8443 | - | - |
| PostgreSQL (internal only by default) | postgres | 5432 | securecloud | securecloud |
| Redis (internal only by default) | redis | 6379 | - | - |
| Adminer (admin profile / db-adminer target) | localhost | 9090 | - | - |

> Change all credentials in production (`config/env/prod/.env`).
> By default, only the gateway is published to the host. Other services stay on the internal Docker network.

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/QUICK_START_FR.md](docs/QUICK_START_FR.md) | 3-minute quick start (French) |
| [docs/INSTALL_FR.md](docs/INSTALL_FR.md) | Full installation guide (French) |
| [docs/DOCKER_GUIDE.md](docs/DOCKER_GUIDE.md) | Docker Compose reference |
| [docs/architecture.md](docs/architecture.md) | System architecture |
| [docs/security.md](docs/security.md) | Security model |

---

## License

MIT License.
