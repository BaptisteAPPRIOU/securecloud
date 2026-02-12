# Guide d'Installation - SecureCloud

Guide complet pour installer et configurer l'environnement de développement SecureCloud.

## Prérequis

### Windows avec MSYS2

1. **Installer MSYS2** : <https://www.msys2.org/>
2. **Installer Docker Desktop** : <https://www.docker.com/products/docker-desktop/>
3. **Installer Qt 6.8+** : <https://www.qt.io/download-qt-installer> (installer dans `C:\Qt\6.8.1\mingw_64`)
4. **Ouvrir MSYS2 UCRT64** (icône violette, pas MinGW64, pas MSYS)

> **Important** : Utilisez toujours le terminal **UCRT64** pour éviter les erreurs de compilation.

## Installation des Dépendances

### Méthode Recommandée (CMake)

```bash
# Mettre à jour la base de paquets
pacman -Syu

# Installer tous les outils nécessaires
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-spdlog \
  mingw-w64-ucrt-x86_64-fmt \
  mingw-w64-ucrt-x86_64-postgresql \
  mingw-w64-ucrt-x86_64-gtest
```

### Configuration du PATH (VS Code / PowerShell)

Si vous utilisez VS Code ou PowerShell (hors terminal UCRT64), ajoutez UCRT64 au PATH :

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
```

Pour rendre permanent, ajoutez à votre profil PowerShell (`$PROFILE`).

## Configuration de la Base de Données

### 1. Configurer le projet

```bash
cmake -B build/dev --preset dev
```

### 2. Lancer PostgreSQL

```bash
cmake --build build/dev --target db-up
```

PostgreSQL sera accessible sur `localhost:15432`

### 3. Executer les migrations

```bash
cmake --build build/dev --target db-migrate
```

### Si erreur sur db-migrate

Exécutez les migrations manuellement **depuis PowerShell** :

```powershell
$envFile = "config/env/dev/.env"
$composeCore = "ops/compose/compose.core.yml"
$composeFull = "ops/compose/compose.full.yml"

docker compose --project-directory . --env-file $envFile -f $composeCore -f $composeFull run --rm flyway-auth
docker compose --project-directory . --env-file $envFile -f $composeCore -f $composeFull run --rm flyway-messaging
docker compose --project-directory . --env-file $envFile -f $composeCore -f $composeFull run --rm flyway-files
docker compose --project-directory . --env-file $envFile -f $composeCore -f $composeFull run --rm flyway-audit
```

Ou depuis **MSYS2** :

```bash
ENV_FILE="config/env/dev/.env"
COMPOSE_CORE="ops/compose/compose.core.yml"
COMPOSE_FULL="ops/compose/compose.full.yml"

docker compose --project-directory . --env-file $ENV_FILE -f $COMPOSE_CORE -f $COMPOSE_FULL run --rm flyway-auth
docker compose --project-directory . --env-file $ENV_FILE -f $COMPOSE_CORE -f $COMPOSE_FULL run --rm flyway-messaging
docker compose --project-directory . --env-file $ENV_FILE -f $COMPOSE_CORE -f $COMPOSE_FULL run --rm flyway-files
docker compose --project-directory . --env-file $ENV_FILE -f $COMPOSE_CORE -f $COMPOSE_FULL run --rm flyway-audit
```

## Interface Web de la Base de Donnees (Adminer)

### Lancer Adminer

```bash
cmake --build build/dev --target db-adminer
```

### Accéder à l'interface

Ouvrez votre navigateur : **<http://localhost:9090>**

### Identifiants de connexion

| Champ    | Valeur          |
| -------- | --------------- |
| System   | PostgreSQL      |
| Server   | postgres        |
| Username | securecloud     |
| Password | securecloud     |
| Database | securecloud_dev |

## Compilation des Services

### Compilation complete (recommande)

```bash
# Configure avec Ninja (rapide)
cmake -B build/dev --preset dev

# Compile tout avec 16 coeurs CPU (~2 minutes)
cmake --build build/dev -j 16
```

### Composants specifiques

```bash
cmake --build build/dev --target gateway        # Gateway
cmake --build build/dev --target auth-service   # Auth Service
cmake --build build/dev --target MSF_Login      # Client Qt
```

### Presets disponibles

| Preset             | Description                             |
| ------------------ | --------------------------------------- |
| `dev`              | Développement complet (Debug, Ninja)    |
| `dev-makefiles`    | Alternative si Ninja n'est pas installé |
| `dev-gateway-only` | Gateway uniquement                      |
| `dev-auth-only`    | Auth Service uniquement                 |
| `dev-client-only`  | Client Qt uniquement                    |
| `release`          | Build optimisé (Release)                |

### Builds Independants

Chaque sous-projet peut etre compile separement avec son propre preset :

```bash
# Gateway uniquement
cd gateway
cmake -B build/gateway-dev --preset gateway-dev
cmake --build build/gateway-dev

# Auth Service uniquement
cd services/auth-service
cmake -B build/auth-dev --preset auth-dev
cmake --build build/auth-dev

# Qt Client uniquement
cd client/qt-app
cmake -B build/client-dev --preset client-dev
cmake --build build/client-dev
```

## Execution des Tests

```bash
# Executer tous les tests
ctest --test-dir build/dev --output-on-failure

# Tests Gateway uniquement
ctest --test-dir build/dev -R gateway

# Tests avec details
ctest --test-dir build/dev -V
```

## Utilisation avec Docker Compose

### Lancer tous les services

```bash
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml up -d --build
```

### Voir les logs

```bash
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml logs -f gateway
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml logs -f auth-service
```

### Arrêter les services

```bash
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml down --remove-orphans
```

## Commandes CMake Utiles

| Commande                                        | Description                |
| ----------------------------------------------- | -------------------------- |
| `cmake -B build/dev --preset dev`               | Configure le projet        |
| `cmake --build build/dev -j 16`                 | Compile tout (parallele)   |
| `cmake --build build/dev --target help-targets` | Liste toutes les cibles    |
| `cmake --build build/dev --target db-up`        | Lance PostgreSQL           |
| `cmake --build build/dev --target db-down`      | Arrete PostgreSQL          |
| `cmake --build build/dev --target db-migrate`   | Execute les migrations     |
| `cmake --build build/dev --target db-reset`     | Reinitialise la DB         |
| `cmake --build build/dev --target db-adminer`   | Lance Adminer              |
| `cmake --build build/dev --target db-psql`      | Connexion psql interactive |
| `cmake --build build/dev --target run-gateway`  | Lance la Gateway           |
| `cmake --build build/dev --target run-client`   | Lance le Client Qt         |
| `ctest --test-dir build/dev`                    | Execute les tests          |

## Configuration (.env)

Le fichier `config/env/dev/.env` contient :

```env
# Base de données
DB_NAME=securecloud_dev
DB_USER=securecloud
DB_PASS=securecloud
DB_PORT=15432
POSTGRES_PORT=15432
DB_HOST=127.0.0.1

# JWT
JWT_SECRET=dev_secret_key_change_in_production_min_32_chars
JWT_ISSUER=securecloud-dev
JWT_EXPIRY_MINUTES=60

# Services
AUTH_SERVICE_URL=http://localhost:8001
GATEWAY_HTTP_PORT=8080
LOG_LEVEL=debug
```

## Dépannage

### "Ninja not found"

Installez Ninja :

```bash
pacman -S mingw-w64-ucrt-x86_64-ninja
```

Ou utilisez le preset de repli : `cmake -B build --preset dev-makefiles`

### Erreurs "at_quick_exit / quick_exit / timespec_get"

Vous utilisez le mauvais environnement (MINGW64 au lieu de UCRT64). Vérifiez :

1. Utilisez le terminal **UCRT64** (icône violette), pas MINGW64
2. Le PATH contient `C:\msys64\ucrt64\bin` en premier
3. Nettoyez et reconfigurez :

```bash
rm -rf build
cmake -B build/dev --preset dev
```

### Build tres lent (30+ minutes)

Vous utilisez probablement MinGW Makefiles. Passez a Ninja :

```bash
rm -rf build
cmake -B build/dev --preset dev          # Utilise Ninja
cmake --build build/dev -j 16            # Build parallele (~2 min)
```

### "Generator doesn't match"

Nettoyez le repertoire build :

```bash
rm -rf build
cmake -B build/dev --preset dev
```

### PostgreSQL ne demarre pas

```bash
# Verifier les logs
cmake --build build/dev --target db-logs

# Verifier que Docker est lance
docker ps

# Reinitialiser
cmake --build build/dev --target db-reset
```

### Erreur "port is already allocated" (8443, 9090, etc.)

Si vous aviez une ancienne installation, des conteneurs historiques peuvent encore entrer en conflit:

- Ancien conteneur PostgreSQL: `sc_pg`
- Stack actuelle: `ops/compose/compose.core.yml` (conteneur `sc_postgres`)

```powershell
# Identifier le conteneur qui occupe un port
docker ps --format "table {{.Names}}\t{{.Ports}}" | findstr 8443
docker ps --format "table {{.Names}}\t{{.Ports}}" | findstr 9090

# Supprimer le conteneur en conflit (exemples)
docker rm -f sc_gateway
docker rm -f compose-adminer-1

# Relancer la stack principale
docker compose --project-directory . --env-file config/env/dev/.env -f ops/compose/compose.core.yml up -d --build
```

### Qt non trouvé

Assurez-vous que Qt est installé dans `C:\Qt\6.8.1\mingw_64` ou modifiez `CMAKE_PREFIX_PATH` dans CMakePresets.json.

### Docker n'est pas reconnu

Assurez-vous que Docker Desktop est lancé et que vous êtes dans le terminal UCRT64.

## Architecture

```text
secureCloud/
├── gateway/              # API Gateway (C++, Boost.Beast)
├── services/
│   ├── auth-service/     # Service d'authentification
│   ├── audit-service/    # Service d'audit
│   ├── files-service/    # Gestion des fichiers
│   ├── messaging-service/# Messagerie temps réel
│   └── deploy-service/   # Déploiement
├── client/
│   └── qt-app/          # Application Qt
├── db/
│   └── migrations/      # Migrations Flyway
├── ops/
│   ├── docker/          # Dockerfiles
│   └── compose/         # Docker Compose
└── config/
    └── env/             # Fichiers .env
```

## Sécurité en Développement

**Important** : Les identifiants par défaut sont pour le développement uniquement.

En production, changez :

- `DB_PASS`
- `JWT_SECRET` (min 32 caractères)
- `REDIS_PASSWORD`
- Activez TLS (`ENABLE_TLS=true`)

## Prochaines Étapes

1. [V] Installer les dépendances
2. [V] Configurer la base de données
3. [V] Compiler les services
4. [ ] Lire la documentation API dans `docs/api/`
5. [ ] Exécuter les tests
6. [ ] Commencer le développement !

## Support

- Documentation : `docs/`
- Architecture : `docs/architecture.md`
- Sécurité : `docs/security.md`

---
