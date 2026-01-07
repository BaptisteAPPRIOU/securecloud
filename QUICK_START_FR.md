# Démarrage Rapide - SecureCloud (CMake)

## Prérequis

- **MSYS2** avec environnement **UCRT64** (pas MINGW64)
- **Ninja** (système de build rapide)
- **Docker Desktop**
- **Qt 6.8+** (pour le client)

> **Important** : Utilisez toujours le terminal **UCRT64** (icône violette), pas MINGW64.

---

## ⚠️ Configuration VS Code

**Pour les utilisateurs de VS Code** : Lors de l'utilisation de l'extension CMake :
1. Ouvrez le fichier workspace : `securecloud.code-workspace`
2. Sélectionnez **"SecureCloud (Root)"** dans la barre d'état lors de la configuration
3. Cela garantit que CMake trouve les presets dans [CMakePresets.json](CMakePresets.json) racine
4. Les dossiers individuels (Gateway, Auth Service, etc.) ont leurs propres presets locaux

---

## En 3 Minutes

### 1⃣ Configuration initiale

```bash
# Configurer le projet avec CMake (utilise Ninja par défaut)
cmake -B build --preset dev
```

### 2⃣ Lancer la base de données

```bash
cmake --build build --target db-up
cmake --build build --target db-migrate
```

### 3⃣ Compiler le projet

```bash
# Compiler tout (utilise 16 cœurs CPU)
cmake --build build -j 16

# Ou compiler un composant spécifique
cmake --build build --target gateway
cmake --build build --target auth-service
cmake --build build --target MSF_Login
```

### 4⃣ Lancer un service

```bash
cmake --build build --target run-gateway
```

---

## Configuration avec Presets

### Presets disponibles

| Preset             | Description                                |
| ------------------ | ------------------------------------------ |
| `dev`              | Développement complet (Debug, Ninja)       |
| `dev-makefiles`    | Alternative si Ninja n'est pas installé  |
| `dev-gateway-only` | Gateway uniquement                         |
| `dev-auth-only`    | Auth Service uniquement                    |
| `dev-client-only`  | Client Qt uniquement                       |
| `release`          | Build optimisé (Release)                   |

### Builds Indépendants par Service

Chaque service peut être compilé indépendamment :

```bash
# Depuis le dossier d'un service spécifique
cd services/auth-service
cmake -B build --preset auth-dev
cmake --build build

# Depuis le dossier Gateway
cd gateway
cmake -B build --preset gateway-dev
cmake --build build

# Depuis le dossier Client Qt
cd client/qt-app
cmake -B build --preset client-dev
cmake --build build
```

**Presets disponibles par service** :
- `gateway-dev` (dossier gateway/)
- `auth-dev` (services/auth-service/)
- `files-dev` (services/files-service/)
- `messaging-dev` (services/messaging-service/)
- `audit-dev` (services/audit-service/)
- `client-dev` (client/qt-app/)

### Utilisation des presets

```bash
# Configuration avec un preset
cmake -B build --preset dev

# Build avec parallélisation
cmake --build build -j 16

# Tests
ctest --test-dir build
```

---

## Commandes Disponibles

### Voir toutes les commandes

```bash
cmake --build build --target help-targets
```

### Base de données

```bash
cmake --build build --target db-up        # Démarrer PostgreSQL
cmake --build build --target db-down      # Arrêter PostgreSQL
cmake --build build --target db-migrate   # Exécuter les migrations
cmake --build build --target db-reset     # Réinitialiser la DB
cmake --build build --target db-adminer   # Lancer Adminer
cmake --build build --target db-logs      # Voir les logs
cmake --build build --target db-psql      # Session psql interactive
```

### Docker

```bash
cmake --build build --target docker-up      # Démarrer tous les services
cmake --build build --target docker-down    # Arrêter tous les services
cmake --build build --target docker-status  # Voir le statut
cmake --build build --target docker-logs    # Voir les logs
```

### Exécution

```bash
cmake --build build --target run-gateway  # Lancer la Gateway
cmake --build build --target run-auth     # Lancer Auth Service
cmake --build build --target run-client   # Lancer le Client Qt
```

### Nettoyage

```bash
cmake --build build --target clean-all    # Nettoyer build + volumes Docker
```

---

## Voir la Base de Données

### Lancer Adminer

```bash
cmake --build build --target db-adminer
```

### Ouvrir l'interface

Accédez à <http://localhost:8080>

### Se connecter

| Champ    | Valeur            |
| -------- | ----------------- |
| System   | `PostgreSQL`      |
| Server   | `postgres`        |
| Username | `securecloud`     |
| Password | `securecloud`     |
| Database | `securecloud_dev` |

---

## Installation des Dépendances MSYS2

### Ouvrir le bon terminal

Lancez **MSYS2 UCRT64** (icône violette) depuis le menu Démarrer.

### Installer les paquets

```bash
# Mettre à jour la base de paquets
pacman -Syu

# Installer les outils de build
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

---

## Résolution des Problèmes

### "Ninja not found"

Installez Ninja :

```bash
pacman -S mingw-w64-ucrt-x86_64-ninja
```

Ou utilisez le preset de repli : `cmake -B build --preset dev-makefiles`

### Erreurs "at_quick_exit / quick_exit / timespec_get"

Vous utilisez le mauvais environnement (MINGW64 au lieu de UCRT64). Vérifiez :

1. Utilisez le terminal UCRT64, pas MINGW64
2. Le PATH contient `C:\msys64\ucrt64\bin` en premier
3. Nettoyez et reconfigurez : `rm -rf build && cmake -B build --preset dev`

### Build très lent (30+ minutes)

Vous utilisez probablement MinGW Makefiles. Passez à Ninja :

```bash
rm -rf build
cmake -B build --preset dev          # Utilise Ninja
cmake --build build -j 16            # Build parallèle
```

### "Generator doesn't match"

Nettoyez le répertoire build :

```bash
rm -rf build
cmake -B build --preset dev
```

---

## Configuration VS Code

### Extensions recommandées

- CMake Tools (`ms-vscode.cmake-tools`)
- CMake (`twxs.cmake`)

### Utilisation avec CMake Tools

1. Ouvrir la palette de commandes (Ctrl+Shift+P)
2. Sélectionner "CMake: Select Configure Preset"
3. Choisir "dev" ou un autre preset
4. CMake Tools s'occupe du reste !

---

## Workflows

### Workflow de développement complet

```bash
# Configuration + Build + Tests
cmake --workflow --preset full-dev
```

### Workflow de release

```bash
# Configuration + Build optimisé
cmake --workflow --preset release-build
```

---

## Compilation spécifique

### Gateway uniquement

```bash
cmake -B build --preset dev-gateway-only
cmake --build build
```

### Client Qt uniquement

```bash
cmake -B build --preset dev-client-only
cmake --build build
```

---

## Vérifier que tout fonctionne

```bash
# 1. Configurer et compiler
cmake -B build --preset dev
cmake --build build

# 2. Vérifier PostgreSQL
cmake --build build --target docker-status

# 3. Lancer la DB et migrer
cmake --build build --target db-up
cmake --build build --target db-migrate

# 4. Ouvrir psql
cmake --build build --target db-psql

# Dans psql:
\dt auth.*
\dt messaging.*
\dt files.*
\dt audit.*
\q
```

---

## Structure du projet

```text
SecureCloud/
├── CMakeLists.txt          # Configuration racine CMake
├── CMakePresets.json       # Presets de configuration
├── build/                  # Dossier de build (créé par CMake)
│   └── dev/               # Build pour le preset "dev"
├── gateway/               # Service Gateway (C++)
├── services/
│   ├── auth-service/     # Service d'authentification
│   ├── files-service/    # Service de fichiers
│   ├── messaging-service/# Service de messagerie
│   └── audit-service/    # Service d'audit
├── client/qt-app/        # Application Qt
└── ops/compose/          # Configuration Docker Compose
```

---

## Pour en savoir plus

 Guide complet : [INSTALL_FR.md](INSTALL_FR.md)  
 Guide Docker : [DOCKER_GUIDE.md](DOCKER_GUIDE.md)  
Architecture : [docs/architecture.md](docs/architecture.md)

---

## Migration depuis Makefile

| Ancien (make)       | Nouveau (CMake)                             |
| ------------------- | ------------------------------------------- |
| `make db-up`        | `cmake --build build --target db-up`        |
| `make db-migrate`   | `cmake --build build --target db-migrate`   |
| `make build-gateway`| `cmake --build build --target gateway`      |
| `make build-auth`   | `cmake --build build --target auth-service` |
| `make build-client` | `cmake --build build --target MSF_Login`    |
| `make run-gateway`  | `cmake --build build --target run-gateway`  |
| `make clean`        | `cmake --build build --target clean`        |
| `make status`       | `cmake --build build --target docker-status`|
