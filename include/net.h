#ifndef NET_H
#define NET_H
#include "game.h"

#define MAX_USERNAME_LEN 50

enum { DRAW = 0, CONTINUE = 1 };

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LEN];
    int player_id;
} Client;

int apply_move_from_pit(int player, int pit_index);

#endif
