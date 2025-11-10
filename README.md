# 🎮 Awale Client-Serveur

> Jeu multijoueur **Awale (Oware)** en C avec architecture client-serveur TCP

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Network](https://img.shields.io/badge/network-TCP%2FIP-green.svg)

---

## 📝 Description

Implémentation complète du jeu traditionnel Awale avec une architecture réseau robuste. Le serveur gère plusieurs parties simultanées, un système de matchmaking, un classement ELO, et permet aux spectateurs de regarder les parties en cours.

### ✨ Fonctionnalités Principales

#### 🎯 Jeu & Matchmaking
- **Règles complètes du jeu Awale** avec validation serveur
- **Système de défis** entre joueurs
- **Multijoueur** : Jusqu'à 100 clients simultanés
- **Mode spectateur** : Jusqu'à 10 spectateurs par partie

#### 📊 Système de Classement
- **Score ELO** : Classement dynamique des joueurs
- Points gagnés/perdus selon victoires/défaites
- Parties entre amis **n'affectent pas l'ELO**

#### 👥 Fonctionnalités Sociales
- **Système d'amis** : Demandes, acceptation, gestion de liste
- **Chat en temps réel** : Messages publics et privés
- **Profils personnalisés** : Bio de 10 lignes max
- **Mode privé** : Parties visibles uniquement par vos amis

#### 💾 Persistance & Historique
- **Sauvegarde des parties** (manuelle ou automatique)
- **Replay complet** : Historique des 200 derniers coups
- **Reconnexion** : Retrouvez vos données (ELO, amis) après déconnexion

#### 🎨 Interface Client
- **Affichage en couleurs** (ANSI)
- **Saisie non-bloquante** : Les messages entrants ne coupent pas votre texte
- **Interface intuitive** avec aide intégrée

---

## 🚀 Installation & Utilisation

### Prérequis

- **Compilateur C** (gcc, clang)
- **Make**
- **Linux/macOS** (ou WSL sur Windows)

### Compilation

```bash
make
```

Les binaires sont générés dans `bin/` :
- `bin/server` - Serveur de jeu
- `bin/client` - Client joueur

### Lancer le serveur

```bash
./bin/server
```

Le serveur démarre sur **port 4321** et affiche :
```
Server on 4321
```

### Lancer un client

#### En local (même machine)
```bash
./bin/client 127.0.0.1 4321
```

#### Sur le réseau
```bash
./bin/client <IP_DU_SERVEUR> 4321
```

**Exemple :** Si le serveur est sur `192.168.1.100` :
```bash
./bin/client 192.168.1.100 4321
```

#### Première connexion

Lors de la connexion, entrez votre **nom d'utilisateur** :
- Minimum 2 caractères
- Lettres, chiffres, `_` ou `-` uniquement
- Unique (pas de doublon)

---

## 🕹️ Jouer sur 2 Machines Différentes

### Sur la machine SERVEUR

1. **Lancer le serveur** :
   ```bash
   ./bin/server
   ```

2. **Trouver l'IP du serveur** :
   ```bash
   hostname -I
   ```
   Exemple de résultat : `192.168.1.100`

3. **Ouvrir le pare-feu** (si nécessaire) :
   ```bash
   sudo ufw allow 4321/tcp
   ```

### Sur la machine CLIENT

```bash
./bin/client 192.168.1.100 4321
```

Remplacez `192.168.1.100` par l'IP réelle du serveur.

---

## 📖 Guide des Commandes

### 💬 Syntaxe Générale

| Syntaxe | Action |
|---------|--------|
| `<texte>` | Message de chat (contexte dépendant) |
| `/<commande>` | Exécuter une commande |
| `@<username> <message>` | Message privé |

### 🌐 Commandes Globales

| Commande | Description |
|----------|-------------|
| `/help` | Afficher l'aide complète |
| `/list` | Liste des joueurs (triés par ELO ↓) |
| `/games` | Liste des parties en cours |
| `/board` | Afficher le plateau de jeu |

### 👤 Profil & Social

| Commande | Description |
|----------|-------------|
| `/bio` | Éditer votre bio (10 lignes max) |
| `/whois <username>` | Voir la bio d'un joueur |
| `/addfriend <username>` | Envoyer une demande d'ami |
| `/acceptfriend <username>` | Accepter une demande d'ami |
| `/friendrequests` | Voir les demandes reçues |
| `/removefriend <username>` | Retirer un ami |
| `/friends` | Afficher votre liste d'amis |

### ⚙️ Modes & Paramètres

| Commande | Description |
|----------|-------------|
| `/private` | Toggle mode privé (parties visibles amis uniquement) |
| `/save` | Toggle sauvegarde automatique des parties |

### 🎮 Lobby (Hors Partie)

| Commande | Description |
|----------|-------------|
| `/challenge <username>` | Défier un joueur |
| `/accept <username>` | Accepter un défi |
| `/refuse <username>` | Refuser un défi |
| `/watch <id>` | Regarder la partie `<id>` (spectateur) |
| `<message>` | Message public (tous les joueurs en ligne) |
| `@<username> <msg>` | Message privé |

### 🕹️ En Partie

| Commande | Description |
|----------|-------------|
| `/0` à `/11` | Jouer un coup (numéro de case) |
| `/d` | Proposer l'égalité à l'adversaire |
| `/q` | Abandonner (forfait) |
| `/board` | Réafficher le plateau |
| `<message>` | Message à l'adversaire et spectateurs |
| `@<username> <msg>` | Message privé |

### 👁️ Mode Spectateur

| Commande | Description |
|----------|-------------|
| `/stopwatch` | Quitter le mode spectateur |
| `/board` | Réafficher le plateau |
| `<message>` | Message aux joueurs et spectateurs |

### 📜 Historique des Parties

| Commande | Description |
|----------|-------------|
| `/history` | Liste des 20 dernières parties sauvegardées |
| `/replay <numéro>` | Revoir une partie (historique complet) |

---

## 💾 Système de Sauvegarde

### Modes de Sauvegarde

#### 🔄 Mode Automatique
```bash
/save  # Toggle ON/OFF
```
- Toutes vos parties sont automatiquement sauvegardées
- Pas de question posée à la fin
- Pratique pour les joueurs réguliers

#### 🤔 Mode Manuel (par défaut)
- À la fin de chaque partie, demande : **"Sauvegarder cette partie ? (o/n)"**
- Partie sauvegardée si **au moins un joueur** répond "oui"
- Utile pour ne garder que les parties importantes

### Format des Fichiers

Les parties sont sauvegardées dans `saved_games/` :
```
game_20251110_143022_Alice_vs_Bob.txt
```

**Contenu :**
- Métadonnées (date, joueurs, résultat)
- Scores finaux
- **Historique complet** des 200 derniers coups
- Graines capturées à chaque coup

### Déconnexion en Partie

**Si un joueur se déconnecte :**
- L'adversaire **gagne par forfait**
- Notification immédiate : `"<Joueur> s'est déconnecté. Vous gagnez!"`
- Si `/save` actif → sauvegarde automatique
- Sinon → demande de sauvegarde à l'adversaire restant

---

## 🎲 Règles du Jeu Awale

### Plateau Initial

```
      P2 (adversaire)
  11  10   9   8   7   6
[ 4][ 4][ 4][ 4][ 4][ 4]
[ 4][ 4][ 4][ 4][ 4][ 4]
   0   1   2   3   4   5
      P1 (vous)
```

Chaque case contient **4 graines** au départ (48 graines total).

### Déroulement d'un Tour

1. **Choisir une case** de votre camp (0-5 pour P1, 6-11 pour P2)
2. **Distribuer les graines** dans le sens anti-horaire (←)
3. **Capturer** si la dernière graine tombe dans le camp adverse :
   - Si la case contient maintenant **2 ou 3 graines** → capture
   - Continue de capturer les cases précédentes tant qu'elles ont 2-3 graines

### Conditions de Victoire

- **Majorité** : Le joueur avec **le plus de graines capturées** gagne
- **Égalité** : Si 24-24 (ou accord mutuel avec `/d`)
- **Forfait** : Si l'adversaire abandonne (`/q`) ou se déconnecte

### Fin de Partie

La partie se termine quand :
- Un joueur ne peut plus jouer (cases vides)
- Les deux joueurs acceptent l'égalité
- Un joueur abandonne

---

## 📊 Système de Classement ELO

### Score Initial

Chaque nouveau joueur commence avec **100 points ELO**.

### Gains & Pertes

| Résultat | Variation ELO |
|----------|---------------|
| **Victoire** | +1 point |
| **Défaite** | -1 point (minimum 0) |
| **Égalité** | 0 point |

### Règle Importante : Parties Entre Amis

🔒 **Les parties contre un ami N'AFFECTENT PAS l'ELO**

- Seules les parties contre des **non-amis** comptent pour le classement
- Permet de jouer librement avec vos amis sans risque
- Encouragez les matchs compétitifs avec des inconnus

### Affichage du Classement

```bash
/list
```

**Résultat** (trié par ELO décroissant) :
```
=== Joueurs disponibles ===
  • Alice(150)
  • Bob(120)
  • Charlie(100)
  • David(95)
===========================
```

---

## 🔄 Système de Reconnexion

### Données Persistantes

Si vous vous déconnectez puis reconnectez (serveur toujours actif), vous retrouvez :

- ✅ **Score ELO**
- ✅ **Liste d'amis**
- ✅ **Demandes d'amis en attente**
- ✅ **Votre bio**
- ✅ **Modes activés** (privé, sauvegarde)

### Reconnexion

```bash
./bin/client 127.0.0.1 4321
# Entrez le MÊME username
```

**Message de bienvenue :**
```
Bon retour Alice! (ELO: 150)
```

### Limitation

⚠️ **Données perdues si le serveur redémarre** (pas de persistance sur disque)

---

## 🏗️ Architecture & Structure

### Architecture Réseau

```
┌─────────────┐
│   Client 1  │──┐
└─────────────┘  │
                 │
┌─────────────┐  │    ┌──────────────┐
│   Client 2  │──┼───→│   SERVEUR    │
└─────────────┘  │    │  (port 4321) │
                 │    └──────────────┘
┌─────────────┐  │
│   Client N  │──┘
└─────────────┘
```

**Protocole** : TCP/IP (connexion fiable)  
**Multiplexage** : `select()` (gestion concurrente de 100+ clients)  
**Autorité** : Le serveur valide tous les coups (anti-triche)

### Structure des Fichiers

```
awale-client-server/
├── README.md              # Ce fichier
├── Makefile               # Compilation
├── .gitignore             # Fichiers ignorés par git
│
├── include/               # Headers (.h)
│   ├── game.h            # Logique du jeu Awale
│   └── net.h             # Utilitaires réseau
│
├── src/
│   ├── common/           # Code partagé
│   │   ├── game.c       # Implémentation des règles
│   │   └── net.c        # Fonctions réseau
│   │
│   ├── server/
│   │   └── server.c     # Main du serveur
│   │
│   └── client/
│       └── client.c     # Main du client
│
├── bin/                  # Binaires (ignoré par git)
│   ├── server
│   └── client
│
└── saved_games/          # Parties sauvegardées (ignoré par git)
    └── game_*.txt
```

### Séparation des Responsabilités

| Fichier | Responsabilité |
|---------|----------------|
| **`game.h/c`** | Règles du jeu, validation des coups |
| **`net.h/c`** | Communication réseau (sockets, messages) |
| **`server.c`** | Gestion des clients, parties, matchmaking |
| **`client.c`** | Interface utilisateur, affichage, saisie |

---

## 🔧 Commandes Make

```bash
make           # Compiler le serveur et le client
make clean     # Supprimer les binaires
make server    # Compiler uniquement le serveur
make client    # Compiler uniquement le client
```

---

## 🐛 Développement & Git

### Workflow Git

```bash
# Vérifier le statut
git status

# Ajouter les modifications
git add .

# Commiter avec un message
git commit -m "Description des changements"

# Pousser sur GitHub
git push
```

### En cas de conflits

```bash
# Récupérer les dernières modifications
git pull

# Si conflits détectés
git status  # Voir les fichiers en conflit

# Résoudre dans VS Code (interface graphique)
# Puis valider la résolution
git add .
git commit -m "Résolution des conflits"
git push
```

### Branches (recommandé)

```bash
# Créer une branche pour une feature
git checkout -b feature/nouvelle-fonctionnalite

# Travailler sur la branche
git add .
git commit -m "Ajout de la fonctionnalité X"
git push origin feature/nouvelle-fonctionnalite

# Fusionner dans main (via Pull Request sur GitHub)
```

---

## 📚 Concepts de Programmation Réseau

### Points Clés

| Concept | Implémentation |
|---------|----------------|
| **TCP vs UDP** | TCP pour garantir ordre & fiabilité |
| **`select()`** | Multiplexage I/O (1 thread, N clients) |
| **Protocole textuel** | Messages terminés par `\n` (facile à déboguer) |
| **Autorité serveur** | Validation côté serveur (anti-triche) |
| **Non-bloquant** | Saisie client sans blocage sur messages entrants |

### Questions Fréquentes

**Q: Pourquoi `select()` et pas threads ?**  
R: Plus simple, pas de race conditions, suffisant pour <100 clients

**Q: Comment détecter une déconnexion ?**  
R: `recv()` retourne 0 → client déconnecté

**Q: Pourquoi un protocole textuel ?**  
R: Facile à déboguer (`telnet localhost 4321`), suffisant pour Awale

**Q: Comment gérer 2 joueurs qui jouent en même temps ?**  
R: `select()` traite séquentiellement → le serveur rejette le 2e coup

---

## 📄 Licence

Ce projet est développé dans le cadre d'un projet académique.

---

## 👥 Contributeurs

- [@diegoaquinoh](https://github.com/diegoaquinoh)
- [@mlemseffer](https://github.com/mlemseffer)