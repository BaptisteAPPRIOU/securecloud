ENV_FILE = config/env/dev/.env
COMPOSE  = docker compose --env-file $(ENV_FILE) -f ops/compose/compose.dev.yml

.PHONY: db-up db-migrate db-down db-reset
db-up:
	$(COMPOSE) up -d postgres

db-migrate:
	bash ops/scripts/db_migrate_all.sh

db-down:
	$(COMPOSE) down

db-reset:
	bash ops/scripts/db_reset.sh
