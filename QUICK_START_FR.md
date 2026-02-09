 # Démarrage Rapide - SecureCloud (CMake)

 ## Prérequis

 - **MSYS2** (UCRT64)
 - **Ninja**
 - **Docker Desktop**
 - **Qt 6.8+** (pour le client)

 > **Important** : Utilisez le terminal **MSYS2 UCRT64** (icône violette).

 ---

 ## En 3 minutes (essentiel)

 1) Configurer le projet :

 ```bash
 cmake -B build/dev --preset dev
 ```

 2) Lancer la base de données (une seule fois) :

 ```bash
 cmake --build build/dev --target db-up
 cmake --build build/dev --target db-migrate
 ```

 3) Compiler et lancer un service (exemple Gateway) :

 ```bash
 cmake --build build/dev --target gateway
 cmake --build build/dev --target run-gateway
 ```

 ---

 ## Quick Checks & Links

 - Pour les détails (presets, tâches VS Code, troubleshooting), voir `README.md`.
 - Checklist rapide pour les nouveaux arrivants : assurez-vous que `cmake --build build/dev --target db-up` et `db-migrate` fonctionnent, puis démarrez `run-dev.ps1` pour le service ciblé.

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

 Pour plus de commandes et explications, référez-vous au fichier `README.md`.
