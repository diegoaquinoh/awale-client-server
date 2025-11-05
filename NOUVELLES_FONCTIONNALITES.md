# Nouvelles Fonctionnalités Awale Client-Server

## Fonctionnalités Implémentées

### 1. Enregistrement des Clients
- Chaque client s'enregistre avec un **username** unique
- Le serveur accueille le joueur avec son nom

### 2. Système de Lobby
- Les clients peuvent demander la **liste des joueurs en ligne** disponibles
- Commande: `list`

### 3. Système de Défi
- **Client A peut défier Client B**
  - Commande: `challenge <nom_joueur>`
- **Client B peut accepter ou refuser**
  - Accepter: `accept <nom_joueur>`
  - Refuser: `refuse <nom_joueur>`

### 4. Attribution Aléatoire du Premier Joueur
- Quand un match est créé entre A et B, le **serveur décide aléatoirement qui commence**
- Les rôles P1 (pits 0-5) et P2 (pits 6-11) sont attribués aléatoirement

### 5. Support Multi-Parties
- Le serveur peut gérer **jusqu'à 10 clients simultanés**
- Plusieurs parties peuvent se dérouler en même temps
- Les joueurs peuvent rejouer après une partie terminée

## Commandes Client

### En Lobby (Hors Partie)
```
list                - Affiche la liste des joueurs disponibles
challenge <nom>     - Défie un joueur spécifique
accept <nom>        - Accepte le défi d'un joueur
refuse <nom>        - Refuse le défi d'un joueur
```

### En Partie
```
<numéro>            - Joue une case (0-5 pour P1, 6-11 pour P2)
d                   - Propose une égalité
```

## Protocole de Communication

### Messages Serveur → Client
- `REGISTER` - Demande d'enregistrement
- `USERLIST <nom1> <nom2> ...` - Liste des joueurs disponibles
- `CHALLENGED_BY <nom>` - Notification de défi reçu
- `ROLE <0|1>` - Attribution du rôle dans la partie
- `STATE <board> <scores> <current>` - État du jeu
- `MSG <message>` - Message informatif
- `END <draw|winner X>` - Fin de partie
- `ASKDRAW` - Demande de réponse pour égalité

### Messages Client → Serveur
- `USERNAME <nom>` - Envoi du nom d'utilisateur
- `LIST` - Demande la liste des joueurs
- `CHALLENGE <nom>` - Envoie un défi
- `ACCEPT <nom>` - Accepte un défi
- `REFUSE <nom>` - Refuse un défi
- `MOVE <pit>` - Joue un coup
- `DRAW` - Propose une égalité
- `YES/NO` - Réponse à une demande d'égalité

## Exemple d'Utilisation

### Terminal 1 (Serveur)
```bash
./bin/server
```

### Terminal 2 (Client Alice)
```bash
./bin/client 127.0.0.1 4321
Entrez votre nom d'utilisateur: Alice
> list
=== Joueurs disponibles ===
  - Bob
===========================
> challenge Bob
MSG Défi envoyé. En attente de réponse...
```

### Terminal 3 (Client Bob)
```bash
./bin/client 127.0.0.1 4321
Entrez votre nom d'utilisateur: Bob
*** Alice vous défie! ***
Tapez 'accept Alice' pour accepter ou 'refuse Alice' pour refuser
> accept Alice
MSG Partie commencée! Vous êtes P2 (pits 6..11). Adversaire: Alice
```

## Compilation
```bash
make clean
make
```

## Architecture Technique

### Structures de Données
- `Client`: Contient socket, username, statut, adversaire
- `Game`: Contient l'état de chaque partie (board, scores, joueurs)
- `ClientStatus`: CONNECTED, WAITING, IN_GAME

### Gestion des États
1. **CONNECTED**: Client vient de se connecter
2. **WAITING**: Client disponible pour défier ou être défié
3. **IN_GAME**: Client en partie

### Synchronisation
- Utilisation de `select()` pour multiplexage I/O
- Le serveur gère tous les clients dans une seule boucle événementielle
- Chaque partie maintient son propre état indépendant
