$EnvFile    = "config/env/dev/.env"
$ComposeYml = "ops/compose/compose.dev.yml"
docker compose --env-file $EnvFile -f $ComposeYml down -v
