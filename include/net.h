#ifndef NET_H
#define NET_H
#include "game.h"

#define MAX_USERNAME_LEN 30
#define MAX_CLIENTS 30
#define MAX_SPECTATORS 20
#define MAX_BIO_LINES 10
#define MAX_BIO_LINE_LEN 80
#define MAX_FRIENDS 20

enum { DRAW = 0, CONTINUE = 1 };

typedef enum {
    CLIENT_CONNECTED,
    CLIENT_WAITING,
    CLIENT_IN_GAME,
    CLIENT_SPECTATING,
    CLIENT_EDITING_BIO,
    CLIENT_ASKED_SAVE
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
    char friends[MAX_FRIENDS][MAX_USERNAME_LEN];  // Liste d'amis
    int num_friends;     // Nombre d'amis
    char friend_requests[MAX_FRIENDS][MAX_USERNAME_LEN];  // Demandes d'amis reçues
    int num_friend_requests;  // Nombre de demandes en attente
    int private_mode;    // Mode privé activé (1) ou non (0)
    int save_mode;       // Mode sauvegarde activé (1) ou non (0)
    int save_response;   // Réponse à la demande de sauvegarde: -1=pas de réponse, 0=non, 1=oui
    int game_to_save;    // Index de la partie à sauvegarder (-1 si aucune)
    int elo_score;       // Score ELO du joueur (100 par défaut)
} Client;

int apply_move_from_pit(int player, int pit_index);

#endif
