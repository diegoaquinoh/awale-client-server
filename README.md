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
