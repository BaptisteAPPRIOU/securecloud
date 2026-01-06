# 🚀 Guide d'Installation - SecureCloud

Guide complet pour installer et configurer l'environnement de développement SecureCloud.

## 📋 Prérequis

### Windows avec MSYS2

1. **Installer MSYS2** : https://www.msys2.org/
2. **Installer Docker Desktop** : https://www.docker.com/products/docker-desktop/
3. **Ouvrir MSYS2 UCRT64** (pas MinGW64, pas MSYS)

## ⚙️ Installation des Dépendances

### Méthode Automatique
```bash
make install-deps
# ou
mingw32-make install-deps
```

### Méthode Manuelle
```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-libpqxx \
  mingw-w64-ucrt-x86_64-postgresql-libs \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-fmt \
  mingw-w64-ucrt-x86_64-spdlog \
  mingw-w64-ucrt-x86_64-qt6
```

## 🗄️ Configuration de la Base de Données

### 1. Copier le fichier de configuration
```bash
cp config/env/dev/.env.example config/env/dev/.env
```

### 2. Lancer PostgreSQL
```bash
make db-up
# ou
mingw32-make db-up
```

PostgreSQL sera accessible sur `localhost:15432`

### 3. Exécuter les migrations
```bash
make db-migrate
# ou
mingw32-make db-migrate
```

### ⚠️ Si `make` n'est pas détecté
Utilisez `mingw32-make` à la place de `make` :
```bash
mingw32-make db-up
mingw32-make db-migrate
```

### ⚠️ Si erreur sur db-migrate

Exécutez les migrations manuellement **depuis PowerShell** :

```powershell
$envFile = "config/env/dev/.env"
$compose = "ops/compose/compose.dev.yml"

docker compose --env-file $envFile -f $compose run --rm flyway-auth
docker compose --env-file $envFile -f $compose run --rm flyway-messaging
docker compose --env-file $envFile -f $compose run --rm flyway-files
docker compose --env-file $envFile -f $compose run --rm flyway-audit
```

Ou depuis **MSYS2** :
```bash
ENV_FILE="config/env/dev/.env"
COMPOSE="ops/compose/compose.dev.yml"

docker compose --env-file $ENV_FILE -f $COMPOSE run --rm flyway-auth
docker compose --env-file $ENV_FILE -f $COMPOSE run --rm flyway-messaging
docker compose --env-file $ENV_FILE -f $COMPOSE run --rm flyway-files
docker compose --env-file $ENV_FILE -f $COMPOSE run --rm flyway-audit
```

## 🌐 Interface Web de la Base de Données (Adminer)

### Lancer Adminer
```bash
make db-adminer
# ou
mingw32-make db-adminer
```

### Accéder à l'interface
Ouvrez votre navigateur : **http://localhost:8080**

### Identifiants de connexion
| Champ    | Valeur           |
|----------|------------------|
| System   | PostgreSQL       |
| Server   | postgres         |
| Username | securecloud      |
| Password | securecloud      |
| Database | securecloud_dev  |

## 🏗️ Compilation des Services

### Gateway
```bash
make build-gateway
# ou
cd gateway
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Service d'Authentification
```bash
make build-auth
# ou
cd services/auth-service
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Client Qt
```bash
make build-client
# ou
cd client/qt-app
cmake -B build
cmake --build build
```

## 🧪 Exécution des Tests

### Tests Gateway
```bash
make test-gateway
# ou
cd gateway/build
ctest --output-on-failure
```

### Tests Auth Service
```bash
make test-auth
# ou
cd services/auth-service/build
ctest --output-on-failure
```

## 🐳 Utilisation avec Docker Compose

### Lancer tous les services
```bash
docker compose up -d
```

### Voir les logs
```bash
docker compose logs -f gateway
docker compose logs -f auth-service
```

### Arrêter les services
```bash
docker compose down
```

## 📝 Commandes Make Utiles

| Commande | Description |
|----------|-------------|
| `make help` | Affiche toutes les commandes disponibles |
| `make setup` | Configuration initiale (.env) |
| `make db-up` | Lance PostgreSQL |
| `make db-down` | Arrête PostgreSQL |
| `make db-migrate` | Exécute toutes les migrations |
| `make db-reset` | Réinitialise la base de données |
| `make db-adminer` | Lance l'interface Adminer |
| `make db-logs` | Affiche les logs PostgreSQL |
| `make db-psql` | Connexion psql interactive |
| `make status` | Affiche l'état des services |
| `make clean` | Nettoie les fichiers de build |
| `make clean-all` | Nettoie tout (build + volumes) |

## 🔧 Configuration (.env)

Le fichier `config/env/dev/.env` contient :

```env
# Base de données
DB_NAME=securecloud_dev
DB_USER=securecloud
DB_PASS=securecloud
DB_PORT=5432
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

## 🐛 Dépannage

### PostgreSQL ne démarre pas
```bash
# Vérifier les logs
make db-logs

# Réinitialiser
make db-reset
```

### Migrations échouent
```bash
# Vérifier que PostgreSQL est démarré
make status

# Exécuter une migration spécifique
make db-migrate-auth
```

### Erreur de compilation
```bash
# Nettoyer et recompiler
make clean
make build-gateway
```

### Docker n'est pas reconnu
Assurez-vous que Docker Desktop est lancé et que vous êtes dans MSYS2 UCRT64.

### Make n'est pas reconnu
Utilisez `mingw32-make` à la place de `make` :
```bash
mingw32-make db-up
```

## 📚 Architecture

```
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

## 🔐 Sécurité en Développement

⚠️ **Important** : Les identifiants par défaut sont pour le développement uniquement.

En production, changez :
- `DB_PASS`
- `JWT_SECRET` (min 32 caractères)
- `REDIS_PASSWORD`
- Activez TLS (`ENABLE_TLS=true`)

## 🚀 Prochaines Étapes

1. ✅ Installer les dépendances
2. ✅ Configurer la base de données
3. ✅ Compiler les services
4. 📖 Lire la documentation API dans `docs/api/`
5. 🧪 Exécuter les tests
6. 💻 Commencer le développement !

## 📞 Support

- Issues GitHub : [Créer une issue](#)
- Documentation : `docs/`
- Architecture : `docs/architecture.md`
- Sécurité : `docs/security.md`

---

**Bon développement ! 🎉**
