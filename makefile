ENV_FILE = config/env/dev/.env
COMPOSE  = docker compose --env-file $(ENV_FILE) -f ops/compose/compose.dev.yml

ifeq ($(OS),Windows_NT)
SHELL := pwsh.exe
.SHELLFLAGS := -NoProfile -ExecutionPolicy Bypass -Command
MIGRATE := ops/scripts/db_migrate_all.ps1
RESET   := ops/scripts/db_reset.ps1
else
SHELL := /bin/bash
MIGRATE := ops/scripts/db_migrate_all.sh
RESET   := ops/scripts/db_reset.sh
endif

.PHONY: db-up db-migrate db-down db-reset
db-up:      ; $(COMPOSE) up -d postgres
db-migrate: ; $(MIGRATE)
db-down:    ; $(COMPOSE) down
db-reset:   ; $(RESET)
