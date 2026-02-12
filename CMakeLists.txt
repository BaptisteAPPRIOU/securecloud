# =============================================================================
# SecureCloud - Root CMakeLists.txt
# =============================================================================
# Usage:
#   cmake --preset dev                        # Configure (outputs to build/dev/)
#   cmake --build build/dev                   # Build all
#   cmake --build build/dev --target db-up    # Start database
#   cmake --build build/dev --target help-targets
# =============================================================================
cmake_minimum_required(VERSION 3.24)

# =============================================================================
# CMake Policies
# =============================================================================
# CMP0135: Set timestamps of extracted files to time of extraction
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
# CMP0167: Use Boost's CMake config instead of FindBoost
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(SecureCloud
    VERSION 1.0.0
    DESCRIPTION "Secure Cloud Platform with Gateway, Auth, Files, Messaging, Audit services and Qt Client"
    LANGUAGES CXX
)

# =============================================================================
# Global Options
# =============================================================================
option(BUILD_GATEWAY "Build the Gateway service" ON)
option(BUILD_AUTH_SERVICE "Build the Auth service" ON)
option(BUILD_FILES_SERVICE "Build the Files service" OFF)
option(BUILD_MESSAGING_SERVICE "Build the Messaging service" OFF)
option(BUILD_AUDIT_SERVICE "Build the Audit service" OFF)
option(BUILD_QT_CLIENT "Build the Qt Client application" ON)
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    enable_testing()
endif()

# =============================================================================
# C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Platform Detection & Configuration
# =============================================================================
if(WIN32)
    set(IS_WINDOWS TRUE)
    set(SHELL_CMD "cmd.exe")
    set(SHELL_ARGS "/C")
    set(PATH_SEP "\\")
    set(EXE_SUFFIX ".exe")
else()
    set(IS_WINDOWS FALSE)
    set(SHELL_CMD "/bin/sh")
    set(SHELL_ARGS "-c")
    set(PATH_SEP "/")
    set(EXE_SUFFIX "")
endif()

# MSYS2/MinGW Configuration
if(MINGW AND NOT CMAKE_PREFIX_PATH)
    if(EXISTS "C:/msys64/ucrt64")
        list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
        message(STATUS "Added MSYS2 UCRT64 to CMAKE_PREFIX_PATH")
    endif()
endif()

# Qt path for client (uses MSYS2 UCRT64 Qt by default)
set(QT_PATH "C:/msys64/ucrt64" CACHE PATH "Path to Qt installation")

# =============================================================================
# Environment Configuration
# =============================================================================
set(_default_env_file "${CMAKE_SOURCE_DIR}/config/env/dev/.env")
if (NOT EXISTS "${_default_env_file}")
    set(_default_env_file "${CMAKE_SOURCE_DIR}/.env")
endif()

set(SECURECLOUD_ENV_FILE
    "${_default_env_file}"
    CACHE FILEPATH "Environment file used by Docker Compose targets"
)
set(_default_core_compose_file "${CMAKE_SOURCE_DIR}/ops/compose/compose.core.yml")
set(_default_full_compose_file "${CMAKE_SOURCE_DIR}/ops/compose/compose.full.yml")

set(SECURECLOUD_CORE_COMPOSE_FILE
    "${_default_core_compose_file}"
    CACHE FILEPATH "Compose file used by docker-up-core and docker-build targets"
)
set(SECURECLOUD_FULL_COMPOSE_FILE
    "${_default_full_compose_file}"
    CACHE FILEPATH "Overlay compose file used by docker-up-full (planned/experimental services)"
)
if((NOT EXISTS "${SECURECLOUD_CORE_COMPOSE_FILE}") AND EXISTS "${_default_core_compose_file}")
    set(SECURECLOUD_CORE_COMPOSE_FILE
        "${_default_core_compose_file}"
        CACHE FILEPATH "Compose file used by docker-up-core and docker-build targets" FORCE
    )
endif()
if((NOT EXISTS "${SECURECLOUD_FULL_COMPOSE_FILE}") AND EXISTS "${_default_full_compose_file}")
    set(SECURECLOUD_FULL_COMPOSE_FILE
        "${_default_full_compose_file}"
        CACHE FILEPATH "Overlay compose file used by docker-up-full (planned/experimental services)" FORCE
    )
endif()
set(SECURECLOUD_FULL_PROFILE "full" CACHE STRING "Compose profile used by docker-up-full target")
set(SECURECLOUD_DESKTOP_PROFILE "desktop" CACHE STRING "Compose profile used by docker-up-desktop target")
set(SECURECLOUD_MONITORING_PROFILE "monitoring" CACHE STRING "Compose profile used by docker-up-monitoring target")

# Database configuration defaults
set(DB_HOST "127.0.0.1" CACHE STRING "Database host")
set(DB_PORT "15432" CACHE STRING "Database port")
set(DB_NAME "securecloud_dev" CACHE STRING "Database name")
set(DB_USER "securecloud" CACHE STRING "Database user")
set(DB_PASS "securecloud" CACHE STRING "Database password")

# =============================================================================
# Docker Compose Command
# =============================================================================
find_program(DOCKER_EXECUTABLE
    NAMES docker
    DOC "Docker executable"
)

# Use 'docker compose' (V2) - handles paths with spaces correctly
set(DOCKER_DB_COMPOSE_ARGS compose --project-directory "${CMAKE_SOURCE_DIR}" --env-file "${SECURECLOUD_ENV_FILE}" -f "${SECURECLOUD_CORE_COMPOSE_FILE}" -f "${SECURECLOUD_FULL_COMPOSE_FILE}")
set(DOCKER_STACK_CORE_COMPOSE_ARGS compose --project-directory "${CMAKE_SOURCE_DIR}" --env-file "${SECURECLOUD_ENV_FILE}" -f "${SECURECLOUD_CORE_COMPOSE_FILE}")
set(DOCKER_STACK_FULL_COMPOSE_ARGS compose --project-directory "${CMAKE_SOURCE_DIR}" --env-file "${SECURECLOUD_ENV_FILE}" -f "${SECURECLOUD_CORE_COMPOSE_FILE}" -f "${SECURECLOUD_FULL_COMPOSE_FILE}")

# =============================================================================
# Subprojects
# =============================================================================
if(BUILD_GATEWAY)
    add_subdirectory(gateway)
    message(STATUS "Gateway: ENABLED")
else()
    message(STATUS "Gateway: DISABLED")
endif()

if(BUILD_AUTH_SERVICE)
    add_subdirectory(services/auth-service)
    message(STATUS "Auth Service: ENABLED")
else()
    message(STATUS "Auth Service: DISABLED")
endif()

if(BUILD_FILES_SERVICE)
    add_subdirectory(services/files-service)
    message(STATUS "Files Service: ENABLED")
else()
    message(STATUS "Files Service: DISABLED")
endif()

if(BUILD_MESSAGING_SERVICE)
    add_subdirectory(services/messaging-service)
    message(STATUS "Messaging Service: ENABLED")
else()
    message(STATUS "Messaging Service: DISABLED")
endif()

if(BUILD_AUDIT_SERVICE)
    add_subdirectory(services/audit-service)
    message(STATUS "Audit Service: ENABLED")
else()
    message(STATUS "Audit Service: DISABLED")
endif()

if(BUILD_QT_CLIENT)
    add_subdirectory(client/qt-app)
    message(STATUS "Qt Client: ENABLED")
else()
    message(STATUS "Qt Client: DISABLED")
endif()

# =============================================================================
# Custom Targets - Help
# =============================================================================
add_custom_target(help-targets
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "======================================================"
    COMMAND ${CMAKE_COMMAND} -E echo "SecureCloud - Available CMake Targets"
    COMMAND ${CMAKE_COMMAND} -E echo "======================================================"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Build Targets ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  all              - Build all enabled components"
    COMMAND ${CMAKE_COMMAND} -E echo "  gateway          - Build Gateway only"
    COMMAND ${CMAKE_COMMAND} -E echo "  auth-service     - Build Auth Service only"
    COMMAND ${CMAKE_COMMAND} -E echo "  MSF_Login        - Build Qt Client only"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Database Targets ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  db-up            - Start PostgreSQL container"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-down          - Stop PostgreSQL container"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-migrate       - Run all Flyway migrations"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-migrate-auth  - Migrate auth schema only"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-reset         - Reset database (WARNING: deletes data)"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-adminer       - Start Adminer web interface"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-logs          - Show PostgreSQL logs"
    COMMAND ${CMAKE_COMMAND} -E echo "  db-psql          - Interactive psql session"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Docker Targets ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-build     - Build Docker images from ops/compose/compose.core.yml"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-build-full - Build Docker images from core + full overlay"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-up-core   - Start core stack (gateway + auth + infra)"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-up-full   - Start planned/experimental stack (--profile ${SECURECLOUD_FULL_PROFILE})"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-up-desktop - Start core stack + Qt desktop client (--profile ${SECURECLOUD_DESKTOP_PROFILE})"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-up-monitoring - Start monitoring profile (--profile ${SECURECLOUD_MONITORING_PROFILE})"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-up        - Alias to docker-up-core"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-down      - Stop all Docker services"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-status    - Show Docker services status"
    COMMAND ${CMAKE_COMMAND} -E echo "  docker-logs      - Show Docker logs"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Run Targets ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  run-gateway      - Run the Gateway"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-auth         - Run Auth Service"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-client       - Run Qt Client"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Utility Targets ==="
    COMMAND ${CMAKE_COMMAND} -E echo "  setup            - Initial setup (copy .env)"
    COMMAND ${CMAKE_COMMAND} -E echo "  smoke-auth-session - E2E auth flow smoke test"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean-all        - Clean builds and Docker volumes"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMENT "Displaying available targets"
    VERBATIM
)

# =============================================================================
# Custom Targets - Setup
# =============================================================================
add_custom_target(setup
    COMMAND ${CMAKE_COMMAND} -E echo "Initial setup..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config/env/dev/.env.example"
        "${CMAKE_SOURCE_DIR}/config/env/dev/.env"
    COMMAND ${CMAKE_COMMAND} -E echo "[OK] Environment file ready"
    COMMENT "Setting up development environment"
    VERBATIM
)

find_program(POWERSHELL_EXECUTABLE
    NAMES pwsh powershell
    DOC "PowerShell executable"
)

if(POWERSHELL_EXECUTABLE)
    add_custom_target(smoke-auth-session
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/scripts/smoke-auth-session.ps1"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running auth session smoke test (login -> refresh -> logout -> old token denied)"
        VERBATIM
    )
endif()

# =============================================================================
# Custom Targets - Database
# =============================================================================
if(DOCKER_EXECUTABLE)
    add_custom_target(db-up
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} up -d postgres
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] PostgreSQL started on localhost:${DB_PORT}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting PostgreSQL container"
        VERBATIM
    )

    add_custom_target(db-down
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} stop postgres
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] PostgreSQL stopped"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Stopping PostgreSQL container"
        VERBATIM
    )

    add_custom_target(db-migrate
        COMMAND ${CMAKE_COMMAND} -E echo "Running all migrations..."
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-auth
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-messaging
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-files
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-audit
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] All migrations completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Flyway migrations"
        VERBATIM
    )

    add_custom_target(db-migrate-auth
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-auth
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Auth migration completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running auth schema migration"
        VERBATIM
    )

    add_custom_target(db-migrate-messaging
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-messaging
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Messaging migration completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running messaging schema migration"
        VERBATIM
    )

    add_custom_target(db-migrate-files
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-files
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Files migration completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running files schema migration"
        VERBATIM
    )

    add_custom_target(db-migrate-audit
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-audit
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Audit migration completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running audit schema migration"
        VERBATIM
    )

    add_custom_target(db-reset
        COMMAND ${CMAKE_COMMAND} -E echo "[WARNING] Resetting database - all data will be lost!"
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} down -v postgres
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} up -d postgres
        COMMAND ${CMAKE_COMMAND} -E sleep 5
        COMMAND ${CMAKE_COMMAND} -E echo "Running migrations..."
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-auth
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-messaging
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-files
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} run --rm flyway-audit
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Resetting database"
        VERBATIM
    )

    add_custom_target(db-adminer
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} up -d adminer
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Adminer available at: http://localhost:9090 (default)"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "Connection details:"
        COMMAND ${CMAKE_COMMAND} -E echo "  System:   PostgreSQL"
        COMMAND ${CMAKE_COMMAND} -E echo "  Server:   postgres"
        COMMAND ${CMAKE_COMMAND} -E echo "  Username: ${DB_USER}"
        COMMAND ${CMAKE_COMMAND} -E echo "  Password: ${DB_PASS}"
        COMMAND ${CMAKE_COMMAND} -E echo "  Database: ${DB_NAME}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting Adminer"
        VERBATIM
    )

    add_custom_target(db-logs
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} logs -f postgres
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Showing PostgreSQL logs"
        VERBATIM
    )

    add_custom_target(db-psql
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_DB_COMPOSE_ARGS} exec postgres psql -U ${DB_USER} -d ${DB_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting interactive psql session"
        VERBATIM
    )

    # =============================================================================
    # Custom Targets - Docker
    # =============================================================================
    add_custom_target(docker-build
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_CORE_COMPOSE_ARGS} build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building Docker images for core application stack"
        VERBATIM
    )

    add_custom_target(docker-build-full
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building Docker images for core + planned/experimental stack"
        VERBATIM
    )

    add_custom_target(docker-up-core
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_CORE_COMPOSE_ARGS} up -d
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Core Docker stack started"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting core Docker services"
        VERBATIM
    )

    add_custom_target(docker-up-full
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" up -d
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Full Docker stack started"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting Docker services with core + planned/experimental overlay"
        VERBATIM
    )

    add_custom_target(docker-up-desktop
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_CORE_COMPOSE_ARGS} --profile "${SECURECLOUD_DESKTOP_PROFILE}" up -d
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Core Docker stack with Qt desktop client started"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting Docker services with desktop profile"
        VERBATIM
    )

    add_custom_target(docker-up-monitoring
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_CORE_COMPOSE_ARGS} --profile "${SECURECLOUD_MONITORING_PROFILE}" up -d
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Docker stack with monitoring started"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Starting Docker services with monitoring profile"
        VERBATIM
    )

    add_custom_target(docker-up
        DEPENDS docker-up-core
    )

    add_custom_target(docker-down
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" down --remove-orphans
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] All Docker services stopped"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Stopping all Docker services"
        VERBATIM
    )

    add_custom_target(docker-status
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" ps
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Showing Docker services status"
        VERBATIM
    )

    add_custom_target(docker-logs
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" logs -f
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Showing Docker logs"
        VERBATIM
    )
else()
    message(WARNING "Docker executable not found. Docker/db targets will report an error when invoked.")

    foreach(_docker_target IN ITEMS
        db-up db-down db-migrate db-migrate-auth db-migrate-messaging db-migrate-files db-migrate-audit
        db-reset db-adminer db-logs db-psql
        docker-build docker-build-full docker-up-core docker-up-full docker-up-desktop docker-up-monitoring docker-up docker-down docker-status docker-logs
    )
        add_custom_target(${_docker_target}
            COMMAND ${CMAKE_COMMAND} -E echo "[ERROR] Docker executable not found in PATH."
            COMMAND ${CMAKE_COMMAND} -E echo "Install Docker Desktop, then re-run CMake configure."
            VERBATIM
        )
    endforeach()
endif()

# =============================================================================
# Custom Targets - Run
# =============================================================================
if(BUILD_GATEWAY)
    if(WIN32 AND POWERSHELL_EXECUTABLE)
        add_custom_target(run-gateway
            COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/gateway/run-dev.ps1"
            DEPENDS gateway
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Gateway via gateway/run-dev.ps1"
            VERBATIM
        )
    else()
        add_custom_target(run-gateway
            COMMAND $<TARGET_FILE:gateway>
            DEPENDS gateway
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/gateway
            COMMENT "Running Gateway"
            VERBATIM
        )
    endif()
endif()

if(BUILD_AUTH_SERVICE)
    if(WIN32 AND POWERSHELL_EXECUTABLE)
        add_custom_target(run-auth
            COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/services/auth-service/run-dev.ps1"
            DEPENDS auth-service
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Auth Service via services/auth-service/run-dev.ps1"
            VERBATIM
        )
    else()
        add_custom_target(run-auth
            COMMAND $<TARGET_FILE:auth-service>
            DEPENDS auth-service
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/services/auth-service
            COMMENT "Running Auth Service"
            VERBATIM
        )
    endif()
endif()

if(BUILD_QT_CLIENT)
    add_custom_target(run-client
        COMMAND $<TARGET_FILE:MSF_Login>
        DEPENDS MSF_Login
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/client/qt-app
        COMMENT "Running Qt Client"
        VERBATIM
    )
endif()

# =============================================================================
# Custom Targets - Clean
# =============================================================================
if(DOCKER_EXECUTABLE)
    add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning build directories..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Stopping Docker services and removing volumes..."
        COMMAND "${DOCKER_EXECUTABLE}" ${DOCKER_STACK_FULL_COMPOSE_ARGS} --profile "${SECURECLOUD_FULL_PROFILE}" down -v --remove-orphans
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Full cleanup completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Full cleanup (build + Docker volumes)"
        VERBATIM
    )
else()
    add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning build directories..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "[SKIP] Docker not found; skipping container cleanup"
        COMMAND ${CMAKE_COMMAND} -E echo "[OK] Build cleanup completed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Build cleanup (Docker unavailable)"
        VERBATIM
    )
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
include(GNUInstallDirs)

if(BUILD_GATEWAY)
    install(TARGETS gateway
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(BUILD_AUTH_SERVICE)
    install(TARGETS auth-service
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(BUILD_QT_CLIENT)
    install(TARGETS MSF_Login
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "====================================================")
message(STATUS "SecureCloud Configuration Summary")
message(STATUS "====================================================")
message(STATUS "CMake version:     ${CMAKE_VERSION}")
message(STATUS "Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Docker executable: ${DOCKER_EXECUTABLE}")
message(STATUS "Docker env file:   ${SECURECLOUD_ENV_FILE}")
message(STATUS "DB compose files:  ${SECURECLOUD_CORE_COMPOSE_FILE} + ${SECURECLOUD_FULL_COMPOSE_FILE}")
message(STATUS "Core compose file: ${SECURECLOUD_CORE_COMPOSE_FILE}")
message(STATUS "Full overlay file: ${SECURECLOUD_FULL_COMPOSE_FILE}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  Gateway:         ${BUILD_GATEWAY}")
message(STATUS "  Auth Service:    ${BUILD_AUTH_SERVICE}")
message(STATUS "  Files Service:   ${BUILD_FILES_SERVICE}")
message(STATUS "  Messaging:       ${BUILD_MESSAGING_SERVICE}")
message(STATUS "  Audit Service:   ${BUILD_AUDIT_SERVICE}")
message(STATUS "  Qt Client:       ${BUILD_QT_CLIENT}")
message(STATUS "  Tests:           ${BUILD_TESTING}")
message(STATUS "")
message(STATUS "Run 'cmake --build <build-dir> --target help-targets' for available targets")
message(STATUS "====================================================")
