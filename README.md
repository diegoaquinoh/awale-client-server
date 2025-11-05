# awale-client-server

## Description

Jeu multijoueur Awale (Oware) en C avec architecture client-serveur. Le serveur gère plusieurs parties simultanées, un système de matchmaking, et un mode spectateur.

## Fonctionnalités

- **Jeu Awale** : Implémentation complète des règles du jeu Awale
- **Multijoueur** : Jusqu'à 10 clients simultanés
- **Matchmaking** : Système de défi entre joueurs
- **Mode Spectateur** : Regarder les parties en cours (jusqu'à 20 spectateurs par partie)
- **Chat** : Messagerie privée et publique (en partie et hors partie)

## Compilation

```bash
make
```

## Utilisation

### Lancer le serveur

```bash
./bin/server
```

Le serveur écoute sur le port 4321.

### Lancer un client

```bash
./bin/client
```

Le client se connecte au serveur (localhost:4321) et vous demande votre nom d'utilisateur.

## Commandes disponibles

### Dans le lobby (hors partie)

- `list` : Afficher la liste des joueurs disponibles
- `games` : Afficher la liste des parties en cours
- `challenge <username>` : Défier un joueur
- `accept <username>` : Accepter un défi
- `refuse <username>` : Refuser un défi
- `watch <game_id>` : Regarder une partie en cours (mode spectateur)
- `chat <message>` : Envoyer un message à tous les joueurs en ligne
- `chat @<username> <message>` : Envoyer un message privé à un joueur

### En partie

- `<0-5>` : Jouer un coup (numéro du pit)
- `d` : Proposer l'égalité
- `q` : Abandonner
- `chat <message>` : Envoyer un message à l'adversaire et aux spectateurs

### En mode spectateur

- `stopwatch` : Quitter le mode spectateur
- `chat <message>` : Envoyer un message aux joueurs et spectateurs de la partie

## Règles du jeu Awale

Le plateau comporte 12 cases (pits), 6 par joueur. Chaque case contient initialement 4 graines.

- **Tour de jeu** : Le joueur choisit une case non vide de son camp et distribue les graines dans le sens horaire.
- **Capture** : Si la dernière graine tombe dans le camp adverse et que la case contient maintenant 2 ou 3 graines, le joueur capture ces graines. La capture continue en sens inverse tant que les cases précédentes (dans le camp adverse) contiennent 2 ou 3 graines.
- **Fin de partie** : La partie se termine quand un joueur ne peut plus jouer ou quand les deux joueurs acceptent l'égalité. Le joueur avec le plus de graines gagne.

## File structure:

game-c-client-server/
├─ README.md
├─ Makefile
├─ .gitignore
├─ include/
│  ├─ protocol.h        # message types, wire format
│  ├─ net.h             # socket helpers
│  └─ game.h            # game state & logic API
├─ src/
│  ├─ common/
│  │  ├─ protocol.c
│  │  ├─ net.c
│  │  └─ game.c
│  ├─ server/
│  │  └─ server.c       # main() for server
│  └─ client/
│     └─ client.c       # main() for client
└─ assets/              # optional (maps, sprites, etc. if any)


##git:

git add .
git commit -m "commentaire du commit"
git push

si il y a eu des push sur main :
git pull
git push

si conflits

git merge 

et puis tu ouvres vscode , git et tu resous les conflits et valides et commit (tout sur interface vscode)