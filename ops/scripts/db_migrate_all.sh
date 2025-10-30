#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."/compose
# charge .env si présent
[ -f ../../config/env/dev/.env ] && export $(grep -v '^#' ../../config/env/dev/.env | xargs)

docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml up -d postgres
docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml run --rm flyway-auth
docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml run --rm flyway-messaging
docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml run --rm flyway-files
docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml run --rm flyway-audit
