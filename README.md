# SecureCloud

A secure cloud platform built with C++20, featuring a Gateway, Authentication, Files, Messaging, and Audit services, with a Qt-based desktop client.

## Quick Start

This README is the canonical overview; for quick developer steps see `QUICK_START_FR.md`.

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
#   core: docker-compose.core.yml
#   full overlay (planned/experimental services): docker-compose.full.yml
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
docker compose --env-file config/env/dev/.env -f docker-compose.core.yml up -d --build
```

Important:
- Use `--env-file config/env/dev/.env` to avoid silent fallback to defaults from an empty/missing root `.env`.
- Do not run `db-up`/`db-adminer` stack and `docker-compose.core.yml` stack at the same time unless ports are changed.

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
| PostgreSQL | localhost | 15432 | securecloud | securecloud |
| Adminer (core stack) | localhost | 9090 | - | - |
| Adminer (db-adminer target) | localhost | 9090 | - | - |
| Gateway HTTP | localhost | 8080 | - | - |
| Gateway HTTPS | localhost | 8443 | - | - |
| Auth Service | localhost | 8001 | - | - |

> Change all credentials in production (`config/env/prod/.env`).

---

## Documentation

| Document | Description |
|----------|-------------|
| [QUICK_START_FR.md](QUICK_START_FR.md) | 3-minute quick start (French) |
| [INSTALL_FR.md](INSTALL_FR.md) | Full installation guide (French) |
| [DOCKER_GUIDE.md](DOCKER_GUIDE.md) | Docker Compose reference |
| [docs/architecture.md](docs/architecture.md) | System architecture |
| [docs/security.md](docs/security.md) | Security model |

---

## License

MIT License - See [LICENSE](LICENSE) for details.
