# Tuto de lancement SecureCloud (workflow fiable)

Ce guide décrit le chemin qui fonctionne aujourd'hui pour lancer le projet en local:

- Base de donnees via Docker (`postgres` + `adminer`)
- Migrations Flyway
- Services lances en local: `auth-service`, `gateway`, `MSF_Login` (client Qt)

Important: ce guide se base sur l'etat actuel du repo, pas sur le "full stack docker" complet.

---

## 0) Prerequis

Sur Windows:

- Docker Desktop demarre
- MSYS2 UCRT64 installe (`C:\msys64\ucrt64\bin`)
- CMake + Ninja + toolchain GCC MSYS2
- Qt 6.x (MSYS2 ou installation compatible MinGW)
- OpenSSL disponible dans le `PATH` (necessaire pour generer les certificats gateway)

Option verification rapide:

```powershell
cmake --version
ninja --version
docker --version
docker compose version
qmake --version
openssl version
```

---

## 1) Preparation (premiere fois seulement)

Depuis la racine du projet:

```powershell
cd "C:\Users\tiste\Documents\Cours\Projet final MSF\securecloud"
```

### 1.1 Verifier le fichier d'environnement dev

Le fichier attendu est:

- `config/env/dev/.env`

Assure-toi d'avoir cette ligne (sinon ajoute-la):

```env
AUTH_PORT=8001
```

Pourquoi: `gateway` appelle l'auth sur `localhost:8001`.

### 1.2 Generer les certificats TLS du gateway (si absents)

```powershell
cd gateway
.\scripts\generate_certs.ps1
cd ..
```

Cela cree:

- `gateway/config/certs/dev-cert.pem`
- `gateway/config/certs/dev-key.pem`

---

## 2) Build local des binaires

Toujours depuis la racine:

```powershell
cmake -B build/dev --preset dev
cmake --build build/dev --target gateway auth-service MSF_Login -j 16
```

---

## 3) Docker DB + migrations

## Terminal #1 (laisser ouvert)

Depuis la racine:

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml pull
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml up -d postgres adminer
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml ps
```

Tu dois voir au minimum:

- `sc_pg`
- `compose-adminer-1`

Puis lancer les migrations (toujours terminal #1):

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml run --rm flyway-auth
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml run --rm flyway-messaging
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml run --rm flyway-files
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml run --rm flyway-audit
```

Normal: les conteneurs `flyway-*` sont temporaires et disparaissent apres execution.

Adminer:

- URL: `http://localhost:8080`
- System: `PostgreSQL`
- Server: `postgres`
- Username: valeur `DB_USER` du `.env` (par defaut `securecloud`)
- Password: valeur `DB_PASS` du `.env` (par defaut `securecloud`)
- Database: valeur `DB_NAME` du `.env` (par defaut `securecloud_dev`)

---

## 4) Lancer les services applicatifs

## Terminal #2 (auth-service)

```powershell
cd "C:\Users\tiste\Documents\Cours\Projet final MSF\securecloud"
.\services\auth-service\run-dev.ps1
```

## Terminal #3 (gateway)

```powershell
cd "C:\Users\tiste\Documents\Cours\Projet final MSF\securecloud"
.\gateway\run-dev.ps1
```

## Terminal #4 (client Qt)

```powershell
cd "C:\Users\tiste\Documents\Cours\Projet final MSF\securecloud"
.\client\qt-app\run-dev.ps1
```

---

## 5) Verifications rapides

Dans un terminal libre:

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml ps
Test-NetConnection localhost -Port 15432
curl -k https://localhost:8443/v1/healthz
```

Resultat attendu:

- DB accessible sur `15432`
- Gateway repond `{"status":"ok"}` sur `/v1/healthz`

---

## 6) Routine quotidienne (demarrage rapide)

Si deja build et deja configure:

1. Terminal #1:

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml up -d postgres adminer
```

2. Terminal #2:

```powershell
.\services\auth-service\run-dev.ps1
```

3. Terminal #3:

```powershell
.\gateway\run-dev.ps1
```

4. Terminal #4:

```powershell
.\client\qt-app\run-dev.ps1
```

---

## 7) Arret propre

1. Stopper les applis locales (`Ctrl+C` dans terminaux #2, #3, #4)
2. Puis DB/adminer:

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml down
```

---

## 8) Depannage express

### "gateway ne demarre pas (certificat introuvable)"

Regenerer:

```powershell
cd gateway
.\scripts\generate_certs.ps1
cd ..
```

### "gateway up mais auth KO"

Verifier dans `config/env/dev/.env`:

```env
AUTH_PORT=8001
```

Puis relancer `auth-service` et `gateway`.

### "db migration error"

Verifier d'abord:

```powershell
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml ps
docker compose --env-file config/env/dev/.env -f ops/compose/compose.dev.yml logs -f postgres
```

---

## 9) Note sur Docker Desktop (images vs containers)

- Une **image** telechargee ne veut pas dire "service demarre".
- Ce qui compte pour l'execution: les **containers en cours** (`docker compose ... ps`).
- Dans ce workflow, les containers persistants attendus sont:
  - `sc_pg`
  - `compose-adminer-1`
- Les `flyway-*` sont volontaires en execution one-shot.
