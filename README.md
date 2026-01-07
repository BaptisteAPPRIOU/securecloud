# SecureCloud

A secure cloud platform built with C++20, featuring a Gateway, Authentication, Files, Messaging, and Audit services, with a Qt-based desktop client.

## Quick Start

### Prerequisites

- **Windows**: MSYS2 with UCRT64 toolchain
- **CMake** 3.24+
- **Ninja** (recommended build system)
- **Docker** & Docker Compose
- **Qt 6.8+** (for the client)

---

## Environment Setup (Windows)

### 1. Install MSYS2

Download and install from [msys2.org](https://www.msys2.org/).

### 2. Open UCRT64 Terminal

**Important**: Always use the **UCRT64** shell (not MINGW64 or MSYS):

- Start Menu → "MSYS2 UCRT64" (purple icon)
- Or run: `C:\msys64\ucrt64.exe`

### 3. Install Required Packages

```bash
# Update package database
pacman -Syu

# Install build tools
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-boost \
          mingw-w64-ucrt-x86_64-openssl \
          mingw-w64-ucrt-x86_64-nlohmann-json \
          mingw-w64-ucrt-x86_64-spdlog \
          mingw-w64-ucrt-x86_64-fmt \
          mingw-w64-ucrt-x86_64-postgresql \
          mingw-w64-ucrt-x86_64-gtest
```

### 4. Install Qt 6.8+

Download from [qt.io](https://www.qt.io/download-qt-installer) and install to `C:\Qt\6.8.1\mingw_64`.

### 5. Install Docker Desktop

Download from [docker.com](https://www.docker.com/products/docker-desktop/).

### 6. Configure PATH (VS Code / PowerShell)

If building from VS Code or PowerShell (not UCRT64 terminal), prepend UCRT64 to PATH:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
```

Or add permanently to your PowerShell profile (`$PROFILE`).

---

## Building the Project

> **⚠️ IMPORTANT for VS Code Users**: When using the CMake extension, make sure to:
> 1. Open the workspace file: `securecloud.code-workspace`
> 2. Select **"SecureCloud (Root)"** folder in the status bar when configuring
> 3. This ensures CMake finds the presets in the root [CMakePresets.json](CMakePresets.json)
> 4. Individual folders (Gateway, Auth Service, etc.) have their own local presets

### 1. Configure the project

```bash
cmake -B build --preset dev
```

### 2. Build everything

```bash
cmake --build build -j 16    # Use all CPU cores
```

### 3. Start the database

```bash
cmake --build build --target db-up
cmake --build build --target db-migrate
```

### 4. Run a service

```bash
cmake --build build --target run-gateway
```

---

## Available Presets

| Preset             | Description                                      |
| ------------------ | ------------------------------------------------ |
| `dev`              | Full development build (Debug, Ninja)            |
| `dev-makefiles`    | Fallback if Ninja not installed                  |
| `dev-gateway-only` | Build Gateway only                               |
| `dev-auth-only`    | Build Auth Service only                          |
| `dev-client-only`  | Build Qt Client only                             |
| `release`          | Optimized release build                          |

### Independent Subproject Builds

Each subproject can be built independently using its own preset:

```bash
# Gateway only
cd gateway
cmake -B build --preset gateway-dev
cmake --build build

# Auth Service only
cd services/auth-service
cmake -B build --preset auth-dev
cmake --build build

# Qt Client only
cd client/qt-app
cmake -B build --preset client-dev
cmake --build build
```

**Available service presets**:
- `gateway-dev` (gateway/)
- `auth-dev` (services/auth-service/)
- `files-dev` (services/files-service/)
- `messaging-dev` (services/messaging-service/)
- `audit-dev` (services/audit-service/)
- `client-dev` (client/qt-app/)

**Note**: If `dev` fails with "Ninja not found", install it:

```bash
pacman -S mingw-w64-ucrt-x86_64-ninja
```

Or use:

```bash
--preset dev-makefiles
```

as a slower fallback.

## CMake Targets

### Build Targets

```bash
cmake --build build --target gateway        # Gateway
cmake --build build --target auth-service   # Auth Service
cmake --build build --target MSF_Login      # Qt Client
```

### Database Targets

```bash
cmake --build build --target db-up          # Start PostgreSQL
cmake --build build --target db-down        # Stop PostgreSQL
cmake --build build --target db-migrate     # Run migrations
cmake --build build --target db-reset       # Reset database
cmake --build build --target db-adminer     # Start Adminer UI
cmake --build build --target db-psql        # Interactive psql
```

### Docker Targets

```bash
cmake --build build --target docker-up      # Start all services
cmake --build build --target docker-down    # Stop all services
cmake --build build --target docker-status  # Show status
```

### Run Targets

```bash
cmake --build build --target run-gateway    # Run Gateway
cmake --build build --target run-auth       # Run Auth Service
cmake --build build --target run-client     # Run Qt Client
```

### Help

```bash
cmake --build build --target help-targets   # Show all targets
```

## Project Structure

```text
SecureCloud/
├── CMakeLists.txt          # Root CMake configuration
├── CMakePresets.json       # Build presets
├── gateway/                # C++ API Gateway
├── services/
│   ├── auth-service/       # Authentication service
│   ├── files-service/      # File storage service
│   ├── messaging-service/  # Real-time messaging
│   └── audit-service/      # Audit logging
├── client/qt-app/          # Qt Desktop Client
├── ops/
│   ├── compose/            # Docker Compose files
│   └── docker/             # Dockerfiles
├── config/                 # Configuration files
└── docs/                   # Documentation
```

## VS Code Integration

The workspace is pre-configured with:

- **CMake Tools** integration with presets
- **Build tasks** (Ctrl+Shift+B)
- **Debug configurations** for Gateway, Auth Service, and Qt Client

### Available Tasks

- CMake: Configure
- CMake: Build All
- Build Gateway / Auth Service / Qt Client
- Database: Start / Stop / Migrate / Adminer
- Docker: Up / Down / Status
- Run Gateway / Qt Client

##  Documentation

- [Quick Start (FR)](QUICK_START_FR.md)
- [Installation Guide (FR)](INSTALL_FR.md)
- [Docker Guide](DOCKER_GUIDE.md)
- [Architecture](docs/architecture.md)
- [Security](docs/security.md)

##  Default Credentials

### Database (Development)

| Field    | Value           |
| -------- | --------------- |
| Host     | localhost       |
| Port     | 15432           |
| Database | securecloud_dev |
| Username | securecloud     |
| Password | securecloud     |

### Adminer

Access at <http://localhost:8080> after running `db-adminer` target.

---

## Troubleshooting

### "Ninja not found"

Install Ninja:

```bash
pacman -S mingw-w64-ucrt-x86_64-ninja
```

Or use the fallback preset: `cmake -B build --preset dev-makefiles`

### "at_quick_exit / quick_exit / timespec_get" errors

You're using the wrong toolchain (MINGW64 instead of UCRT64). Ensure:

1. Use UCRT64 terminal, not MINGW64
2. PATH has `C:\msys64\ucrt64\bin` **before** any mingw64 paths
3. Clean rebuild: `rm -rf build && cmake -B build --preset dev`

### Build is very slow (30+ minutes)

You're probably using MinGW Makefiles. Switch to Ninja:

```bash
rm -rf build
cmake -B build --preset dev          # Uses Ninja
cmake --build build -j 16            # Parallel build
```

### "Generator doesn't match"

Clean the build directory and reconfigure:

```bash
rm -rf build
cmake -B build --preset dev
```

### Qt not found

Ensure Qt is installed at `C:\Qt\6.8.1\mingw_64` or update `CMAKE_PREFIX_PATH` in CMakePresets.json.

### PostgreSQL connection refused

Start the database first:

```bash
cmake --build build --target db-up
cmake --build build --target db-migrate
```

---

## License

MIT License - See [LICENSE](LICENSE) for details.
