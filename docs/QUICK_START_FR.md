# Demarrage Rapide - SecureCloud (CMake)

## Prerequis

- MSYS2 (UCRT64)
- Ninja
- Docker Desktop
- Qt 6.8+ (pour le client)

Important: utilisez le terminal MSYS2 UCRT64 (icone violette).

---

## En 3 minutes (essentiel)

1. Configurer le projet:

```bash
cmake -B build/dev --preset dev
```

2. Lancer la base de donnees (une seule fois):

```bash
cmake --build build/dev --target db-up
cmake --build build/dev --target db-migrate
```

3. Compiler et lancer un service (exemple Gateway):

```bash
cmake --build build/dev --target gateway
cmake --build build/dev --target run-gateway
```

---

## Quick Checks & Links

- Commande Docker recommandee:
  `docker compose --env-file config/env/dev/.env -f docker-compose.core.yml up -d --build`
- Pour les details (presets, taches VS Code, troubleshooting), voir `README.md`.
- Checklist rapide: verifier `db-up` et `db-migrate`, puis demarrer `run-dev.ps1` du service cible.

### Tests rapides des services

```powershell
# Gateway
.\gateway\run-dev.ps1

# Auth Service (nouveau terminal)
.\services\auth-service\run-dev.ps1

# Qt Client (nouveau terminal)
.\client\qt-app\run-dev.ps1
```

---

Pour plus de commandes et explications, voir `README.md`.
