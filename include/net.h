#ifndef NET_H
#define NET_H
#include "game.h"

#define MAX_USERNAME_LEN 30
#define MAX_CLIENTS 30
#define MAX_SPECTATORS 20

enum { DRAW = 0, CONTINUE = 1 };

typedef enum {
    CLIENT_CONNECTED,
    CLIENT_WAITING,
    CLIENT_IN_GAME,
    CLIENT_SPECTATING
} ClientStatus;

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LEN];
    int player_id;
    ClientStatus status;
    int opponent_index;  // Index de l'adversaire dans le tableau des clients
    int challenged_by;   // Index du client qui a envoyé un défi (-1 si aucun)
    int watching_game;   // Index de la partie regardée (-1 si aucune)
} Client;

int apply_move_from_pit(int player, int pit_index);

#endif
