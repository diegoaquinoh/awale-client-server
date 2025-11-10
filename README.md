# awale-client-server

## Description

Jeu multijoueur Awale (Oware) en C avec architecture client-serveur. Le serveur gère plusieurs parties simultanées, un système de matchmaking, et un mode spectateur.

## Fonctionnalités

- **Jeu Awale** : Implémentation complète des règles du jeu Awale
- **Multijoueur** : Jusqu'à 10 clients simultanés
- **Matchmaking** : Système de défi entre joueurs
- **Système d'ELO** : Classement des joueurs par score (victoires/défaites)
- **Mode Spectateur** : Regarder les parties en cours (jusqu'à 20 spectateurs par partie)
- **Chat** : Messagerie privée et publique (en partie et hors partie)
- **Profils** : Chaque joueur peut définir une bio (jusqu'à 10 lignes)
- **Système d'amis** : Ajoutez/retirez des amis et gérez votre liste
- **Mode privé** : Limitez l'accès à vos parties aux amis uniquement
- **Historique des parties** : Les parties peuvent être sauvegardées à la fin (sur demande) et revues plus tard

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
./bin/client <ip> <port>
```

Le client se connecte au serveur (localhost:4321) et vous demande votre nom d'utilisateur.

## Commandes disponibles

### Syntaxe générale

**Par défaut, tout ce que vous tapez est envoyé comme message de chat.**  
Pour exécuter une commande, préfixez-la avec `/`  
Pour un message privé, préfixez avec `@username`

### Commandes globales

- `/help` : Afficher la liste des commandes
- `/list` : Afficher la liste des joueurs disponibles (classés par score ELO décroissant)
- `/games` : Afficher la liste des parties en cours
- `/board` : Afficher le plateau (en partie ou spectateur)
- `/bio` : Définir votre bio (jusqu'à 10 lignes ASCII)
- `/whois <username>` : Afficher la bio d'un joueur
- `/history` : Afficher la liste des 20 dernières parties sauvegardées
- `/replay <numéro>` : Revoir une partie sauvegardée (voir l'historique complet des coups)

### Gestion des amis et modes

- `/addfriend <username>` : Envoyer une demande d'ami à un joueur
- `/acceptfriend <username>` : Accepter une demande d'ami reçue
- `/friendrequests` : Afficher les demandes d'amis reçues
- `/removefriend <username>` : Retirer un joueur de votre liste d'amis
- `/friends` : Afficher votre liste d'amis
- `/private` : Activer/désactiver le mode privé (toggle). En mode privé, seuls vos amis peuvent regarder vos parties
- `/save` : Activer/désactiver la sauvegarde automatique (toggle). En mode sauvegarde, vos parties sont automatiquement enregistrées

**Note** : Les relations d'amitié sont réciproques. Pour devenir amis, un joueur doit envoyer une demande avec `/addfriend` et l'autre doit l'accepter avec `/acceptfriend`.

### Dans le lobby (hors partie)

- `/challenge <username>` : Défier un joueur
- `/accept <username>` : Accepter un défi
- `/refuse <username>` : Refuser un défi
- `/watch <game_id>` : Regarder une partie en cours (mode spectateur)
- `@<username> <message>` : Message privé
- `<message>` : Envoyer un message à tous les joueurs en ligne

### En partie

- `/0` à `/11` : Jouer un coup (numéro du pit de 0 à 11)
- `/d` : Proposer l'égalité
- `/q` : Abandonner (forfait)
- `/board` : Réafficher le plateau
- `@<username> <message>` : Message privé
- `<message>` : Envoyer un message à l'adversaire et aux spectateurs

### En mode spectateur

- `/stopwatch` : Quitter le mode spectateur
- `/board` : Réafficher le plateau
- `<message>` : Envoyer un message aux joueurs et spectateurs de la partie

### Déconnexion et sauvegarde

Si un joueur se déconnecte pendant une partie (Ctrl+C, fermeture, etc.), il perd automatiquement et son adversaire gagne par forfait.

**Système de sauvegarde** :
- **Mode automatique** : Activez `/save` pour que toutes vos parties soient automatiquement sauvegardées
- **Mode manuel** : Si aucun joueur n'a activé `/save`, une demande de sauvegarde est proposée à la fin de chaque partie
- Les parties sont enregistrées dans le dossier `saved_games/` avec un nom de fichier contenant la date, l'heure et les noms des joueurs
- Les parties sauvegardées contiennent l'historique complet des coups et peuvent être consultées avec `/history` et `/replay`

## Règles du jeu Awale

Le plateau comporte 12 cases (pits), 6 par joueur. Chaque case contient initialement 4 graines.

- **Tour de jeu** : Le joueur choisit une case non vide de son camp et distribue les graines dans le sens horaire.
- **Capture** : Si la dernière graine tombe dans le camp adverse et que la case contient maintenant 2 ou 3 graines, le joueur capture ces graines. La capture continue en sens inverse tant que les cases précédentes (dans le camp adverse) contiennent 2 ou 3 graines.
- **Fin de partie** : La partie se termine quand un joueur ne peut plus jouer ou quand les deux joueurs acceptent l'égalité. Le joueur avec le plus de graines gagne.

## Système d'ELO

Chaque joueur commence avec un score de **100 points**.

- **Victoire** : +1 point
- **Défaite** : -1 point (minimum 0, le score ne peut pas être négatif)
- **Égalité** : 0 point

**Important** : Les parties entre amis n'affectent **pas** le score ELO. Seules les parties contre des non-amis comptent pour le classement.

La commande `/list` affiche tous les joueurs disponibles triés par score décroissant, au format `username(score)`.

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