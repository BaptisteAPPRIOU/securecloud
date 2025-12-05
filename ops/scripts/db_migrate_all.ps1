$EnvFile    = "config/env/dev/.env"
$ComposeYml = "ops/compose/compose.dev.yml"

docker compose --env-file $EnvFile -f $ComposeYml up -d postgres
docker compose --env-file $EnvFile -f $ComposeYml run --rm flyway-auth
docker compose --env-file $EnvFile -f $ComposeYml run --rm flyway-messaging
docker compose --env-file $EnvFile -f $ComposeYml run --rm flyway-files
docker compose --env-file $EnvFile -f $ComposeYml run --rm flyway-audit