Commandes utiles

Bootstrap vcpkg (à la racine du repo):

git submodule add https://github.com/microsoft/vcpkg.git tools/vcpkg || true
./tools/vcpkg/bootstrap-vcpkg.sh  # .bat sous Windows

Build local du gateway :

cmake --preset dev -S gateway
cmake --build gateway/build -j
./gateway/build/gateway

Docker :

docker build -t gateway:dev -f ops/docker/gateway.Dockerfile .
docker run --rm -p 8443:8443 gateway:dev


A) Windows (MSYS2 MINGW64) + VS Code — recommandé
1) Pré-requis (une seule fois)

Installer MSYS2 : https://www.msys2.org/

Ouvrir MSYS2 MinGW x64 (le terminal avec MINGW64 dans le prompt) et mettre à jour :

pacman -Syu   # redémarrer le terminal si demandé
pacman -Syu


Installer les paquets nécessaires :

pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-openssl \
  mingw-w64-x86_64-spdlog \
  mingw-w64-x86_64-fmt \
  mingw-w64-x86_64-nlohmann-json \
  git


Important : ne pas mélanger UCRT et MINGW. Tous doivent utiliser MINGW64 (pas ucrt64).

2) Cloner et ouvrir le projet

Dans MSYS2 MinGW x64 :

git clone <URL_DU_REPO> securecloud
cd securecloud
code .    # ouvre VS Code avec le bon PATH

3) VS Code – réglages minimalistes

Installe l’extension CMake Tools.
Ajoute à la racine du repo .vscode/settings.json :

{
  "cmake.sourceDirectory": "${workspaceFolder}/gateway",
  "cmake.generator": "Ninja",
  "cmake.buildDirectory": "${workspaceFolder}/gateway/build/mingw",
  "cmake.configureSettings": {
    "CMAKE_BUILD_TYPE": "Debug",
    "CMAKE_C_COMPILER": "gcc",
    "CMAKE_CXX_COMPILER": "g++"
  }
}


Ensuite :

CMake: Select a Kit → choisir GCC x86_64-w64-mingw32 (MINGW64).

Cliquez Configure, puis Build dans la barre CMake.

jwt-cpp est géré par FetchContent dans le CMake du gateway — vos collègues n’ont rien à installer pour ça.
spdlog est forcé en header-only dans le CMake → pas de conflit de DLL.

4) Lancer / Debug

Lancer depuis le terminal MSYS2 :

./gateway/build/mingw/gateway.exe


Debug (facultatif) : ajoute .vscode/launch.json :

{
  "version": "0.2.0",
  "configurations": [{
    "name": "Debug gateway (MINGW64)",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/gateway/build/mingw/gateway.exe",
    "args": ["config/gateway.dev.yaml"],
    "cwd": "${workspaceFolder}/gateway",
    "MIMode": "gdb",
    "miDebuggerPath": "C:/msys64/mingw64/bin/gdb.exe",
    "environment": [{ "name": "PATH", "value": "C:/msys64/mingw64/bin;${env:PATH}" }]
  }]
}

5) Commandes quotidiennes

Après chaque modif de code :

cmake --build gateway/build/mingw -j   # ou bouton Build dans VS Code
./gateway/build/mingw/gateway.exe


Reconfigurer (Configure) seulement si vous changez CMakeLists.txt ou ajoutez des fichiers sources.

6) Dépannage express

find_package introuvable (spdlog/fmt/openssl) → ouvrez VS Code via code . depuis le terminal MINGW64, pas depuis PowerShell/CMD.

Kit UCRT sélectionné par erreur → refaites CMake: Select a Kit et choisissez MINGW64.

DLL manquantes à l’exécution → lancez depuis le terminal MSYS2 MINGW64 (PATH prêt) ou gardez la var PATH du launch.json ci-dessus.