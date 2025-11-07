#ifndef NET_H
#define NET_H
#include "game.h"

#define MAX_USERNAME_LEN 30
#define MAX_CLIENTS 30
#define MAX_SPECTATORS 20
#define MAX_BIO_LINES 10
#define MAX_BIO_LINE_LEN 80

enum { DRAW = 0, CONTINUE = 1 };

typedef enum {
    CLIENT_CONNECTED,
    CLIENT_WAITING,
    CLIENT_IN_GAME,
    CLIENT_SPECTATING,
    CLIENT_EDITING_BIO
} ClientStatus;

typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LEN];
    int player_id;
    ClientStatus status;
    int opponent_index;  // Index de l'adversaire dans le tableau des clients
    int challenged_by;   // Index du client qui a envoyé un défi (-1 si aucun)
    int watching_game;   // Index de la partie regardée (-1 si aucune)
    char bio[MAX_BIO_LINES][MAX_BIO_LINE_LEN];  // Bio du joueur (10 lignes max)
    int bio_lines;       // Nombre de lignes de bio
} Client;

int apply_move_from_pit(int player, int pit_index);

#endif
