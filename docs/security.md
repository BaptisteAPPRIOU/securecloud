# SecureCloud Security Notes

This document is a practical security baseline for local/dev and production-like deployments.

## Core Security Controls

- Authentication: JWT access tokens issued by `auth-service`.
- Authorization: gateway-side RBAC checks before upstream routing.
- Transport: HTTPS available on gateway (`8443`), controlled by `ENABLE_TLS`.
- Secrets: credentials and JWT secret loaded from environment variables.
- Auditability: security-sensitive actions should be logged to `audit-service` (full profile path).

## Mandatory Env Variables

- `DB_PASS`
- `REDIS_PASSWORD`
- `JWT_SECRET` (minimum 32 chars, random, never default in production)

## Dev Defaults vs Production

Dev defaults are intentionally simple (`config/env/dev/.env`) and must not be reused in production.

Production minimum:

```env
DB_PASS=<strong-random-password>
REDIS_PASSWORD=<strong-random-password>
JWT_SECRET=<long-random-secret-at-least-32-chars>
ENABLE_TLS=true
LOG_LEVEL=info
```

## Docker Safety Checks

Always use explicit env and compose file selection:

```bash
docker compose --env-file config/env/dev/.env -f docker-compose.core.yml up -d --build
```

If startup fails with `port is already allocated`, identify and remove conflicting containers:

```powershell
docker ps --format "table {{.Names}}\t{{.Ports}}" | findstr 15432
docker ps --format "table {{.Names}}\t{{.Ports}}" | findstr 8080
docker rm -f <conflicting_container>
```

## Recommended Rotation Policy

- Rotate `JWT_SECRET` and database credentials on schedule.
- Revoke old refresh tokens after secret rotation.
- Keep Docker base images updated (`ubuntu:24.04`, `postgres:16-alpine`, `redis:7-alpine`).

## Scope

This file is an operations-oriented guide. Architecture-level details remain in `docs/architecture.md`.
