#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."/compose
docker compose --env-file ../../config/env/dev/.env -f compose.dev.yml down -v
