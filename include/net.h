#ifndef NET_H
#define NET_H
#include "game.h"

#define MAX_USERNAME_LEN 50
#define MAX_CLIENTS 10

enum { DRAW = 0, CONTINUE = 1 };

typedef enum {
    CLIENT_CONNECTED,
    CLIENT_WAITING,
    CLIENT_IN_GAME
} ClientStatus;

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LEN];
    int player_id;
    ClientStatus status;
    int opponent_index;  // Index de l'adversaire dans le tableau des clients
} Client;

int apply_move_from_pit(int player, int pit_index);

#endif
