# SecureCloud - Makefile
# Usage: make <target> ou mingw32-make <target>

.PHONY: help db-up db-down db-migrate db-reset db-adminer db-logs db-psql setup status clean build-gateway build-auth build-client build-all

# Variables
ENV_FILE := config/env/dev/.env
COMPOSE_FILE := ops/compose/compose.dev.yml
DOCKER_COMPOSE := docker compose --env-file $(ENV_FILE) -f $(COMPOSE_FILE)
QT_PATH := C:/Qt/6.8.1/mingw_64
MINGW_PATH := C:/msys64/mingw64

# Database connection (Docker uses port 15432 mapped to 5432 inside container)
DB_HOST := 127.0.0.1
DB_PORT := 15432
DB_NAME := securecloud_dev
DB_USER := securecloud
DB_PASS := securecloud

# JWT Config
JWT_SECRET := dev-secret-change-me
JWT_ISSUER := securecloud-auth

help: ## Affiche l'aide
	@echo "SecureCloud - Commandes disponibles:"
	@echo ""
	@echo "=== Base de donnees ==="
	@echo "  make setup        - Configuration initiale (.env)"
	@echo "  make db-up        - Lance PostgreSQL"
	@echo "  make db-down      - Arrete PostgreSQL"
	@echo "  make db-migrate   - Execute toutes les migrations"
	@echo "  make db-reset     - Reinitialise la base de donnees"
	@echo "  make db-adminer   - Lance Adminer (interface web)"
	@echo "  make db-logs      - Affiche les logs PostgreSQL"
	@echo "  make db-psql      - Connexion psql interactive"
	@echo "  make status       - Affiche l'etat des services"
	@echo ""
	@echo "=== Compilation ==="
	@echo "  make build-gateway - Compile la Gateway"
	@echo "  make build-auth    - Compile Auth Service"
	@echo "  make build-client  - Compile le client Qt"
	@echo "  make build-all     - Compile tout"
	@echo ""
	@echo "=== Execution ==="
	@echo "  make run-gateway   - Lance la Gateway"
	@echo "  make run-auth      - Lance Auth Service"
	@echo "  make run-client    - Lance le Client Qt"
	@echo ""
	@echo "=== Nettoyage ==="
	@echo "  make clean         - Nettoie les builds"
	@echo "  make clean-all     - Nettoie tout (build + volumes)"
	@echo ""
	@echo "Note: Sur Windows MSYS2, utilisez 'mingw32-make' au lieu de 'make'"

setup: ## Configuration initiale (copie .env)
	@echo Configuration initiale...
	@if not exist "$(ENV_FILE)" ( \
		copy "config\env\dev\.env.example" "$(ENV_FILE)" && \
		echo [OK] Fichier .env cree \
	) else ( \
		echo [INFO] Le fichier .env existe deja \
	)

# ==================== BASE DE DONNEES ====================

db-up: ## Lance le conteneur PostgreSQL
	@echo Demarrage de PostgreSQL...
	$(DOCKER_COMPOSE) up -d postgres
	@echo [OK] PostgreSQL demarre sur localhost:15432

db-down: ## Arrete le conteneur PostgreSQL
	@echo Arret de PostgreSQL...
	$(DOCKER_COMPOSE) down postgres

db-migrate: ## Execute toutes les migrations Flyway
	@echo Execution des migrations...
	@echo Migration auth...
	$(DOCKER_COMPOSE) run --rm flyway-auth
	@echo Migration messaging...
	$(DOCKER_COMPOSE) run --rm flyway-messaging
	@echo Migration files...
	$(DOCKER_COMPOSE) run --rm flyway-files
	@echo Migration audit...
	$(DOCKER_COMPOSE) run --rm flyway-audit
	@echo [OK] Toutes les migrations terminees

db-migrate-auth: ## Migration schema auth uniquement
	$(DOCKER_COMPOSE) run --rm flyway-auth

db-migrate-messaging: ## Migration schema messaging uniquement
	$(DOCKER_COMPOSE) run --rm flyway-messaging

db-migrate-files: ## Migration schema files uniquement
	$(DOCKER_COMPOSE) run --rm flyway-files

db-migrate-audit: ## Migration schema audit uniquement
	$(DOCKER_COMPOSE) run --rm flyway-audit

db-reset: ## Supprime et recree la base de donnees
	@echo [WARNING] Suppression de toutes les donnees...
	$(DOCKER_COMPOSE) down -v postgres
	@echo Redemarrage...
	$(DOCKER_COMPOSE) up -d postgres
	@timeout /t 5 /nobreak >nul
	@echo Migration...
	$(MAKE) db-migrate

db-adminer: ## Lance Adminer (interface web DB)
	@echo Demarrage d'Adminer...
	$(DOCKER_COMPOSE) up -d adminer
	@echo [OK] Adminer disponible sur: http://localhost:8080
	@echo.
	@echo Identifiants de connexion:
	@echo   System:   PostgreSQL
	@echo   Server:   postgres
	@echo   Username: securecloud
	@echo   Password: securecloud
	@echo   Database: securecloud_dev

db-logs: ## Affiche les logs PostgreSQL
	$(DOCKER_COMPOSE) logs -f postgres

db-psql: ## Connexion psql interactive
	$(DOCKER_COMPOSE) exec postgres psql -U securecloud -d securecloud_dev

status: ## Affiche le statut des services
	@echo Etat des services Docker:
	$(DOCKER_COMPOSE) ps

# ==================== COMPILATION ====================

build-gateway: ## Compile la Gateway
	@echo === Compilation Gateway ===
	@if not exist "gateway\build" mkdir gateway\build
	@cd gateway\build && cmake -DCMAKE_PREFIX_PATH="$(MINGW_PATH)" -G "MinGW Makefiles" .. && mingw32-make
	@echo [OK] Gateway compile

build-auth: ## Compile Auth Service
	@echo === Compilation Auth Service ===
	@if not exist "services\auth-service\build" mkdir services\auth-service\build
	@cd services\auth-service\build && cmake -DCMAKE_PREFIX_PATH="$(MINGW_PATH)" -G "MinGW Makefiles" .. && mingw32-make
	@echo [OK] Auth Service compile

build-client: ## Compile le client Qt
	@echo === Compilation Client Qt ===
	@if not exist "client\qt-app\build" mkdir client\qt-app\build
	@cd client\qt-app\build && cmake -DCMAKE_PREFIX_PATH="$(QT_PATH)" -G "MinGW Makefiles" .. && mingw32-make
	@echo [OK] Client Qt compile
	@echo.
	@echo Pour lancer: make run-client

build-all: build-gateway build-auth build-client ## Compile tout
	@echo [OK] Tous les projets compiles

# ==================== EXECUTION ====================

run-client: ## Lance le client Qt
	@if exist "client\qt-app\build\MSF_Login.exe" ( \
		echo [INFO] Lancement du client Qt... && \
		cd client\qt-app\build && MSF_Login.exe \
	) else ( \
		echo [ERREUR] Client non compile. Utilisez: make build-client \
	)

run-gateway: ## Lance la Gateway
	@if exist "gateway\build\gateway.exe" ( \
		echo [INFO] Lancement de la Gateway... && \
		set DB_HOST=$(DB_HOST) && \
		set DB_PORT=$(DB_PORT) && \
		set DB_NAME=$(DB_NAME) && \
		set DB_USER=$(DB_USER) && \
		set DB_PASS=$(DB_PASS) && \
		cd gateway\build && gateway.exe \
	) else ( \
		echo [ERREUR] Gateway non compile. Utilisez: make build-gateway \
	)

run-auth: ## Lance Auth Service
	@if exist "services\auth-service\build\auth-service.exe" ( \
		echo [INFO] Lancement du Auth Service... && \
		echo [INFO] Database: $(DB_HOST):$(DB_PORT)/$(DB_NAME) && \
		set DB_HOST=$(DB_HOST) && \
		set DB_PORT=$(DB_PORT) && \
		set DB_NAME=$(DB_NAME) && \
		set DB_USER=$(DB_USER) && \
		set DB_PASS=$(DB_PASS) && \
		set JWT_SECRET=$(JWT_SECRET) && \
		set JWT_ISSUER=$(JWT_ISSUER) && \
		set AUTH_PORT=8081 && \
		cd services\auth-service\build && auth-service.exe \
	) else ( \
		echo [ERREUR] Auth Service non compile. Utilisez: make build-auth \
	)

# ==================== NETTOYAGE ====================

clean: ## Nettoie les fichiers de build
	@echo Nettoyage...
	@if exist gateway\build rmdir /s /q gateway\build
	@if exist services\auth-service\build rmdir /s /q services\auth-service\build
	@if exist client\qt-app\build rmdir /s /q client\qt-app\build
	@echo [OK] Nettoyage termine

clean-all: clean ## Nettoie tout (build + volumes Docker)
	$(DOCKER_COMPOSE) down -v
	@echo [OK] Nettoyage complet termine
