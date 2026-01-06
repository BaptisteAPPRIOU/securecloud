# Démarrage Rapide - SecureCloud

## En 3 Minutes

### 1️ Configuration (une seule fois)
```bash
# Copier le fichier d'environnement
cp config/env/dev/.env.example config/env/dev/.env
```

### 2️ Lancer la base de données
```bash
make db-up
# ou si "make" n'est pas reconnu :
mingw32-make db-up
```

### 3️ Exécuter les migrations
```bash
make db-migrate
# ou
mingw32-make db-migrate
```

---

## Si ça ne marche pas

### "make n'est pas reconnu"
 Utilisez `mingw32-make` à la place :
```bash
mingw32-make db-up
mingw32-make db-migrate
```

### Erreur sur les migrations

#### Depuis PowerShell
```powershell
# Exécuter le script automatique
.\scripts\migrate.ps1
```

#### Ou manuellement :
```powershell
$envFile = "config/env/dev/.env"
$compose = "ops/compose/compose.dev.yml"

docker compose --env-file $envFile -f $compose run --rm flyway-auth
docker compose --env-file $envFile -f $compose run --rm flyway-messaging
docker compose --env-file $envFile -f $compose run --rm flyway-files
docker compose --env-file $envFile -f $compose run --rm flyway-audit
```

#### Depuis MSYS2/Bash
```bash
# Exécuter le script automatique
./scripts/migrate.sh
```

---

## Voir la Base de Données

### Lancer Adminer
```bash
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml up -d adminer
```

### Ouvrir l'interface
**http://localhost:8080**

### Se connecter
| Champ    | Valeur           |
|----------|------------------|
| System   | `PostgreSQL`     |
| Server   | `postgres`       |
| Username | `securecloud`    |
| Password | `securecloud`    |
| Database | `securecloud_dev`|

---

## Commandes Utiles

```bash
# Voir l'aide
make help

# Lancer Adminer rapidement
make db-adminer

# Voir les logs de la DB
make db-logs

# Réinitialiser la DB
make db-reset

# Arrêter la DB
make db-down

# Statut des services
make status
```

---

## Installation des Dépendances MSYS2

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-libpqxx \
  mingw-w64-ucrt-x86_64-postgresql-libs \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-fmt \
  mingw-w64-ucrt-x86_64-spdlog
```

Ou automatiquement :
```bash
make install-deps
```

---

## Compiler les Services

```bash
# Gateway
make build-gateway

# Service Auth
make build-auth

# Client Qt
make build-client
```

---

## Fichier .env

```env
# Base de données
DB_NAME=securecloud_dev
DB_USER=securecloud
DB_PASS=securecloud
DB_PORT=15432
DB_HOST=127.0.0.1
```

---

## Vérifier que tout fonctionne

```bash
# 1. Vérifier PostgreSQL
make status

# 2. Se connecter à la DB
make db-psql

# 3. Lister les tables
\dt auth.*
\dt messaging.*
\dt files.*
\dt audit.*

# 4. Quitter psql
\q

# 5. Test de connexion
$body = '{ "email":"admin@demo.local", "password":"admin1234" }'

try {
    $response = Invoke-RestMethod -Uri 'http://127.0.0.1:8081/auth/login' -Method POST -ContentType 'application/json' -Body $body
    Write-Host "Login reussi!" -ForegroundColor Green
    Write-Host "Token: $($response.access_token)"
} catch {
    Write-Host "Erreur: $($_.Exception.Message)" -ForegroundColor Red
}
```

---

## Pour en savoir plus

 Guide complet : [INSTALL_FR.md](INSTALL_FR.md)  
 Guide Docker : [DOCKER_GUIDE.md](DOCKER_GUIDE.md)  
 Architecture : [docs/architecture.md](docs/architecture.md)

---