# awale-client-server



##File structure:

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

test