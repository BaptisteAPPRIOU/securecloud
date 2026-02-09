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
| Adminer (DB UI) | localhost | 8080 | - | - |
| Gateway | localhost | 8443 | - | - |
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
