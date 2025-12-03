# SecureCloud Gateway

**Frontale sécurisée pour l'architecture microservices SecureCloud.**

Gateway HTTP/HTTPS avec terminaison TLS, validation JWT, routage dynamique et support WebSocket pour le client Qt.

## 🎯 Fonctionnalités

### ✅ Implémenté
- **Configuration YAML complète** : Server, TLS, routing, upstreams, security, observability
- **TLS/HTTPS** : Terminaison TLS avec OpenSSL, support mTLS optionnel
- **Authentification JWT** : Vérification cryptographique avec jwt-cpp, cache JWKS
- **Cache haute performance** : AuthCache avec TTL et éviction LRU
- **Environnements multiples** : dev/test/prod avec fichiers de config dédiés

### 🔲 À venir (Steps 4-10)
- Connecteurs microservices (HTTP/UDS/WebSocket avec Boost.Beast)
- Routing dynamique avec regex depuis la config
- Endpoints API REST pour client Qt
- Rate limiting et protection DoS
- Observabilité (Prometheus, audit logs JSON)
- Tests complets (unit/intégration/charge/MSF)
- Dockerisation et CI/CD

Voir [IMPLEMENTATION_PROGRESS.md](IMPLEMENTATION_PROGRESS.md) pour le détail.

---

## 🚀 Quick Start

### Prérequis

**Windows (MSYS2 MINGW64)** - Recommandé
```bash
# Installer MSYS2 : https://www.msys2.org/
# Dans MSYS2 MinGW x64 terminal :
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-openssl \
  mingw-w64-x86_64-spdlog \
  mingw-w64-x86_64-fmt \
  mingw-w64-x86_64-nlohmann-json \
  mingw-w64-x86_64-boost \
  git
```

**Linux/macOS**
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake ninja-build libssl-dev \
  libboost-all-dev nlohmann-json3-dev libspdlog-dev libfmt-dev

# macOS (Homebrew)
brew install cmake ninja openssl boost nlohmann-json spdlog fmt
```

### Génération des certificats de développement

```bash
# Windows (PowerShell)
.\scripts\generate_certs.ps1

# Linux/macOS
chmod +x scripts/generate_certs.sh
./scripts/generate_certs.sh
```

Cela génère `config/certs/dev-cert.pem` et `dev-key.pem` auto-signés (365 jours).

⚠️ **Production** : Utiliser Let's Encrypt ou certificats CA corporatifs !

### Build

```bash
# Configuration
cmake --preset dev -S .

# Build
cmake --build build -j

# Ou avec VS Code CMake Tools : Configure → Build
```

### Lancement

```bash
# Depuis la racine du projet
./build/gateway

# Avec config personnalisée
./build/gateway config/gateway.test.yaml
```

Le Gateway écoute sur `0.0.0.0:8443` (HTTPS) par défaut.

---

## 📁 Structure du Projet

```
gateway/
├── config/                  # Fichiers de configuration
│   ├── gateway.dev.yaml     # Développement
│   ├── gateway.test.yaml    # Tests
│   ├── gateway.prod.yaml    # Production
│   └── certs/               # Certificats TLS (ignorés par Git)
├── include/gateway/         # Headers publics
│   ├── config.hpp           # Configuration YAML
│   ├── tlsContext.hpp       # Gestion TLS/OpenSSL
│   ├── jwtFilter.hpp        # Filtre JWT
│   ├── tokenIntrospector.hpp # Vérification JWT + JWKS
│   ├── authCache.hpp        # Cache claims avec TTL/LRU
│   ├── router.hpp           # Routage
│   ├── upstreamProxy.hpp    # Proxy vers microservices
│   └── ...
├── src/                     # Implémentations
├── tests/                   # Tests unitaires et intégration
│   └── UnitTests/
│       ├── test_full_config.cpp
│       ├── test_tls.cpp
│       ├── test_auth_cache.cpp
│       └── ...
├── scripts/                 # Scripts utilitaires
│   ├── generate_certs.sh    # Génération certificats (Bash)
│   ├── generate_certs.ps1   # Génération certificats (PowerShell)
│   └── README.md
├── CMakeLists.txt
├── vcpkg.json              # Dépendances vcpkg
└── README.md
```

---

## 🔧 Configuration

### Fichiers de configuration

Les fichiers YAML définissent tous les aspects du Gateway :

```yaml
server:
  host: 0.0.0.0
  port: 8443
  tls:
    cert_file: config/certs/dev-cert.pem
    key_file: config/certs/dev-key.pem
    client_mtls: false

routing:
  - match: "^/api/login$"
    target: auth
  - match: "^/api/ws$"
    target: messaging
    upgrade: websocket

upstreams:
  auth:
    kind: http
    transport: uds  # Unix Domain Socket
    socket: "/run/securecloud/auth.sock"
  messaging:
    kind: ws
    transport: tcp
    address: "messaging:8081"

security:
  jwks:
    cache_ttl_s: 300

observability:
  prometheus:
    bind: "0.0.0.0:9090"
  logs:
    level: info
```

### Variables d'environnement (Production)

```bash
export GATEWAY_CERT_FILE=/etc/securecloud/certs/prod-cert.pem
export GATEWAY_KEY_FILE=/etc/securecloud/certs/prod-key.pem
export GATEWAY_ENV=prod
```

---

## 🧪 Tests

```bash
# Build des tests
cmake --build build --target unit_tests

# Exécution
./build/tests/UnitTests/unit_tests

# Avec CTest
cd build
ctest --output-on-failure
```

**Tests implémentés** :
- ✅ Configuration parsing (tous environnements)
- ✅ AuthCache (TTL, LRU, thread-safety)
- ✅ TLS context initialization
- 🔲 JWT verification (TODO : générer vrais tokens de test)
- 🔲 Routing (TODO : Step 5)
- 🔲 Integration tests (TODO : Step 9)

---

## 📚 Documentation Technique

### Architecture

Le Gateway agit comme frontale unique exposée au client Qt :

```
[Client Qt] <--HTTPS--> [Gateway] <--IPC--> [Microservices]
                          |
                          |-- auth-service (UDS)
                          |-- messaging-service (TCP/WS)
                          |-- files-service (UDS)
                          |-- audit-service
```

**Flux de requête** :
1. Client Qt → HTTPS/TLS → Gateway
2. Gateway → Validation JWT (cache ou introspector)
3. Gateway → Vérification autorisation (authzFilter)
4. Gateway → Routage (regex match)
5. Gateway → Proxy vers microservice (HTTP/UDS/WS)
6. Microservice → Traitement
7. Gateway ← Réponse microservice
8. Client Qt ← Réponse transformée

### Sécurité

- **TLS 1.2+** : Chiffrement en transit, cipher suites modernes
- **JWT** : Authentification sans état, vérification cryptographique
- **JWKS** : Rotation des clés publiques avec cache (5 min TTL)
- **AuthCache** : Limite les vérifications JWT (performance + DoS protection)
- **mTLS** : Support optionnel pour client certificates
- **Rate Limiting** : TODO - Step 7
- **Audit Logs** : TODO - Step 8

### Performance

**Cibles** :
- ✅ 10 000 messages/sec (objectif MSF)
- ✅ < 100 ms temps de réponse moyen
- ✅ Support 10 000 connexions WebSocket simultanées

**Optimisations** :
- AuthCache avec LRU (évite vérifications JWT répétées)
- Connection pooling vers upstreams (TODO - Step 4)
- Thread pool pour requêtes (existant dans httpServer.cpp)
- JWKS cache (limite appels auth-service)

---

## 🛠️ Développement

### VS Code Setup (Windows MSYS2)

1. **Ouvrir VS Code depuis MSYS2 terminal** :
```bash
cd gateway
code .
```

2. **Extensions requises** :
   - C/C++ (Microsoft)
   - CMake Tools

3. **Settings** (`.vscode/settings.json`) :
```json
{
  "cmake.sourceDirectory": "${workspaceFolder}",
  "cmake.generator": "Ninja",
  "cmake.buildDirectory": "${workspaceFolder}/build/mingw",
  "cmake.configureSettings": {
    "CMAKE_BUILD_TYPE": "Debug",
    "CMAKE_C_COMPILER": "gcc",
    "CMAKE_CXX_COMPILER": "g++"
  }
}
```

4. **Kit Selection** : CMake: Select a Kit → GCC x86_64-w64-mingw32 (MINGW64)

5. **Launch config** (`.vscode/launch.json`) :
```json
{
  "version": "0.2.0",
  "configurations": [{
    "name": "Debug Gateway (MINGW64)",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build/mingw/gateway.exe",
    "args": ["config/gateway.dev.yaml"],
    "cwd": "${workspaceFolder}",
    "MIMode": "gdb",
    "miDebuggerPath": "C:/msys64/mingw64/bin/gdb.exe",
    "environment": [{"name": "PATH", "value": "C:/msys64/mingw64/bin;${env:PATH}"}]
  }]
}
```

### Ajout de nouvelles fonctionnalités

1. **Créer header** dans `include/gateway/`
2. **Créer implémentation** dans `src/`
3. **Ajouter au CMakeLists.txt** : `add_library(gateway_lib ... src/newfile.cpp)`
4. **Créer tests** dans `tests/UnitTests/test_newfeature.cpp`
5. **Documenter** avec commentaires Doxygen

### Guidelines de code

- ✅ C++20, `-Wall -Wextra -Wpedantic`
- ✅ Headers avec documentation complète
- ✅ RAII pour gestion ressources (smart pointers, locks)
- ✅ Thread-safety explicite (mutex, annotations)
- ✅ Logging avec spdlog (debug/info/warn/error)
- ✅ TODO markers pour améliorations futures
- ✅ Tests unitaires pour logique métier

---

## 📖 Références

- [jwt-cpp Documentation](https://thalhammer.github.io/jwt-cpp/)
- [OpenSSL Documentation](https://www.openssl.org/docs/)
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/release/libs/beast/)
- [spdlog Documentation](https://github.com/gabime/spdlog)

---

## 🤝 Contribution

Voir [IMPLEMENTATION_PROGRESS.md](IMPLEMENTATION_PROGRESS.md) pour les tâches en cours.

**Prochaines priorités** :
1. Step 4 : Connecteurs microservices (Boost.Beast)
2. Step 5 : Routing dynamique
3. Step 6 : API REST endpoints

---

## 📄 License

Projet académique - SecureCloud Bachelor 3

---

**Dernière mise à jour** : 1er décembre 2025  
**Statut** : 28.5% complet (Steps 1-3 terminés)
