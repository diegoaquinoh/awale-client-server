#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/game.h"
#include "../../include/net.h"

#define PORT 4321

// Structure pour une partie en cours
typedef struct {
    int client_indices[2];  // Indices des deux joueurs
    char board[12];
    char scores[2];
    char current_player;
    char active;
} Game;

// Variables globales
Client clients[MAX_CLIENTS];
Game games[MAX_CLIENTS / 2];
int num_clients = 0;

/**
 * Reçoit une ligne depuis un socket
 * @param fd Socket file descriptor
 * @param buf Buffer pour stocker la ligne
 * @param cap Capacité du buffer
 * @return Nombre de caractères lus, ou -1 en cas d'erreur
 */
static int recv_line(int fd, char *buf, size_t cap) {
    size_t n = 0;
    char ch;
    
    while (n + 1 < cap) {
        ssize_t r = recv(fd, &ch, 1, 0);
        if (r <= 0) {
            return -1;
        }
        
        if (ch == '\n') {
            buf[n] = 0;
            return (int)n;
        }
        
        buf[n++] = ch;
    }
    
    buf[n] = 0;
    return (int)n;
}
/**
 * Envoie une ligne vers un socket
 * @param fd Socket file descriptor
 * @param s Chaîne à envoyer
 */
static void send_line(int fd, const char* s) {
    send(fd, s, strlen(s), 0);
}

/**
 * Initialise une nouvelle partie
 */
static void init_game_state(Game* g) {
    for (int i = 0; i < 12; i++) {
        g->board[i] = 4;
    }
    g->scores[0] = 0;
    g->scores[1] = 0;
    g->active = 1;
}

/**
 * Trouve l'index d'un client par son socket
 */
static int find_client_by_socket(int fd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd == fd) {
            return i;
        }
    }
    return -1;
}

/**
 * Trouve l'index d'un client par son username
 */
static int find_client_by_username(const char* username) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd > 0 && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Envoie la liste des utilisateurs en ligne à un client
 */
static void send_online_users(int client_idx) {
    char msg[1024] = "USERLIST";
    
    for (int i = 0; i < num_clients; i++) {
        if (i != client_idx && 
            clients[i].socket_fd > 0 && 
            clients[i].status != CLIENT_IN_GAME) {
            strcat(msg, " ");
            strcat(msg, clients[i].username);
        }
    }
    
    strcat(msg, "\n");
    send_line(clients[client_idx].socket_fd, msg);
}

/**
 * Trouve la partie d'un client
 */
static Game* find_game_for_client(int client_idx) {
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        if (games[i].active && 
            (games[i].client_indices[0] == client_idx || 
             games[i].client_indices[1] == client_idx)) {
            return &games[i];
        }
    }
    return NULL;
}

/**
 * Diffuse l'état actuel du jeu aux deux clients d'une partie
 */
static void broadcast_game_state(Game* g) {
    char line[256];
    int c0_idx = g->client_indices[0];
    int c1_idx = g->client_indices[1];
    
    snprintf(line, sizeof(line),
        "STATE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        g->board[0], g->board[1], g->board[2], g->board[3], g->board[4], g->board[5],
        g->board[6], g->board[7], g->board[8], g->board[9], g->board[10], g->board[11],
        g->scores[0], g->scores[1], g->current_player);
    
    send_line(clients[c0_idx].socket_fd, line);
    send_line(clients[c1_idx].socket_fd, line);
}

/**
 * Envoie l'état actuel du jeu à un seul client
 */
static void send_game_state(Game* g, int client_idx) {
    char line[256];
    
    snprintf(line, sizeof(line),
        "STATE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        g->board[0], g->board[1], g->board[2], g->board[3], g->board[4], g->board[5],
        g->board[6], g->board[7], g->board[8], g->board[9], g->board[10], g->board[11],
        g->scores[0], g->scores[1], g->current_player);
    
    send_line(clients[client_idx].socket_fd, line);
}

int main() {
    srand(time(NULL));
    
    // Initialisation des structures
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = -1;
        clients[i].status = CLIENT_CONNECTED;
        clients[i].opponent_index = -1;
        clients[i].challenged_by = -1;
    }
    
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        games[i].active = 0;
    }
    
    // Création du socket serveur
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    
    // Configuration pour réutiliser l'adresse
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configuration de l'adresse du serveur
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(PORT);
    
    // Liaison et écoute
    if (bind(srv, (struct sockaddr*)&a, sizeof(a)) < 0 || listen(srv, 10) < 0) {
        perror("bind/listen");
        return 1;
    }
    
    printf("Server on %d\n", PORT);
    
    // Boucle principale du serveur
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        
        int maxfd = srv;
        
        // Ajouter tous les clients connectés au select
        for (int i = 0; i < num_clients; i++) {
            if (clients[i].socket_fd > 0) {
                FD_SET(clients[i].socket_fd, &rfds);
                if (clients[i].socket_fd > maxfd) {
                    maxfd = clients[i].socket_fd;
                }
            }
        }
        
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) <= 0) {
            continue;
        }
        
        // Nouvelle connexion
        if (FD_ISSET(srv, &rfds)) {
            if (num_clients < MAX_CLIENTS) {
                int new_fd = accept(srv, NULL, NULL);
                if (new_fd >= 0) {
                    clients[num_clients].socket_fd = new_fd;
                    clients[num_clients].status = CLIENT_CONNECTED;
                    clients[num_clients].opponent_index = -1;
                    clients[num_clients].challenged_by = -1;
                    
                    // Demander le username
                    send_line(new_fd, "REGISTER\n");
                    
                    char buf[128];
                    char username[MAX_USERNAME_LEN];
                    int username_valid = 0;
                    
                    if (recv_line(new_fd, buf, sizeof(buf)) > 0) {
                        if (strncmp(buf, "USERNAME ", 9) == 0) {
                            strncpy(username, buf + 9, MAX_USERNAME_LEN - 1);
                            username[MAX_USERNAME_LEN - 1] = '\0';
                            
                            // Vérifier si le username est déjà pris
                            int username_taken = 0;
                            for (int j = 0; j < num_clients; j++) {
                                if (clients[j].socket_fd > 0 && strcmp(clients[j].username, username) == 0) {
                                    username_taken = 1;
                                    break;
                                }
                            }
                            
                            if (username_taken) {
                                send_line(new_fd, "MSG Username déjà pris. Déconnexion.\n");
                                close(new_fd);
                                printf("Connexion refusée: username '%s' déjà pris\n", username);
                            } else {
                                username_valid = 1;
                            }
                        } else {
                            strcpy(username, "Anonymous");
                            username_valid = 1;
                        }
                        
                        if (username_valid) {
                            strcpy(clients[num_clients].username, username);
                            printf("Client connecté: %s\n", clients[num_clients].username);
                            
                            char welcome[128];
                            snprintf(welcome, sizeof(welcome), "MSG Bienvenue %s! Tapez 'list' pour voir les joueurs disponibles.\n", clients[num_clients].username);
                            send_line(new_fd, welcome);
                            
                            clients[num_clients].status = CLIENT_WAITING;
                            num_clients++;
                        }
                    } else {
                        close(new_fd);
                    }
                }
            }
        }
        
        // Traiter les messages des clients existants
        for (int i = 0; i < num_clients; i++) {
            if (clients[i].socket_fd <= 0 || !FD_ISSET(clients[i].socket_fd, &rfds)) {
                continue;
            }
            
            char buf[256];
            if (recv_line(clients[i].socket_fd, buf, sizeof(buf)) <= 0) {
                // Déconnexion
                printf("Client déconnecté: %s\n", clients[i].username);
                close(clients[i].socket_fd);
                clients[i].socket_fd = -1;
                continue;
            }
            
            // Commande LIST - Demander la liste des utilisateurs
            if (!strcmp(buf, "LIST")) {
                send_online_users(i);
            }
            // Commande CHALLENGE - Défier un autre joueur
            else if (!strncmp(buf, "CHALLENGE ", 10)) {
                char target[MAX_USERNAME_LEN];
                strncpy(target, buf + 10, MAX_USERNAME_LEN - 1);
                target[MAX_USERNAME_LEN - 1] = '\0';
                
                int target_idx = find_client_by_username(target);
                
                if (target_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Joueur introuvable.\n");
                } else if (target_idx == i) {
                    send_line(clients[i].socket_fd, "MSG Vous ne pouvez pas vous défier vous-même.\n");
                } else if (clients[target_idx].status == CLIENT_IN_GAME) {
                    send_line(clients[i].socket_fd, "MSG Ce joueur est déjà en partie.\n");
                } else {
                    // Enregistrer le défi
                    clients[target_idx].challenged_by = i;
                    
                    // Envoyer le défi
                    char challenge_msg[128];
                    snprintf(challenge_msg, sizeof(challenge_msg), "CHALLENGED_BY %s\n", clients[i].username);
                    send_line(clients[target_idx].socket_fd, challenge_msg);
                    
                    send_line(clients[i].socket_fd, "MSG Défi envoyé. En attente de réponse...\n");
                }
            }
            // Commande ACCEPT - Accepter un défi
            else if (!strncmp(buf, "ACCEPT ", 7)) {
                char challenger[MAX_USERNAME_LEN];
                strncpy(challenger, buf + 7, MAX_USERNAME_LEN - 1);
                challenger[MAX_USERNAME_LEN - 1] = '\0';
                
                int challenger_idx = find_client_by_username(challenger);
                
                if (challenger_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Joueur introuvable.\n");
                } else if (clients[i].challenged_by != challenger_idx) {
                    // Vérifier que ce joueur a bien envoyé un défi
                    send_line(clients[i].socket_fd, "MSG Ce joueur ne vous a pas défié.\n");
                } else {
                    // Créer une nouvelle partie
                    int game_idx = -1;
                    for (int g = 0; g < MAX_CLIENTS / 2; g++) {
                        if (!games[g].active) {
                            game_idx = g;
                            break;
                        }
                    }
                    
                    if (game_idx == -1) {
                        send_line(clients[i].socket_fd, "MSG Serveur plein.\n");
                        continue;
                    }
                    
                    // Décider aléatoirement qui commence
                    int first_player = rand() % 2;
                    
                    if (first_player == 0) {
                        games[game_idx].client_indices[0] = challenger_idx;
                        games[game_idx].client_indices[1] = i;
                    } else {
                        games[game_idx].client_indices[0] = i;
                        games[game_idx].client_indices[1] = challenger_idx;
                    }
                    
                    init_game_state(&games[game_idx]);
                    games[game_idx].current_player = 0;
                    
                    // Mettre à jour les statuts
                    clients[challenger_idx].status = CLIENT_IN_GAME;
                    clients[challenger_idx].opponent_index = i;
                    clients[i].status = CLIENT_IN_GAME;
                    clients[i].opponent_index = challenger_idx;
                    clients[i].challenged_by = -1;  // Réinitialiser le défi
                    
                    // Attribuer les rôles
                    clients[games[game_idx].client_indices[0]].player_id = 0;
                    clients[games[game_idx].client_indices[1]].player_id = 1;
                    
                    send_line(clients[games[game_idx].client_indices[0]].socket_fd, "ROLE 0\n");
                    send_line(clients[games[game_idx].client_indices[1]].socket_fd, "ROLE 1\n");
                    
                    // Informer les joueurs
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG Partie commencée! Vous êtes P1 (pits 0..5). Adversaire: %s\n", 
                             clients[games[game_idx].client_indices[1]].username);
                    send_line(clients[games[game_idx].client_indices[0]].socket_fd, msg);
                    
                    snprintf(msg, sizeof(msg), "MSG Partie commencée! Vous êtes P2 (pits 6..11). Adversaire: %s\n", 
                             clients[games[game_idx].client_indices[0]].username);
                    send_line(clients[games[game_idx].client_indices[1]].socket_fd, msg);
                    
                    // Envoyer l'état initial
                    broadcast_game_state(&games[game_idx]);
                }
            }
            // Commande REFUSE - Refuser un défi
            else if (!strncmp(buf, "REFUSE ", 7)) {
                char challenger[MAX_USERNAME_LEN];
                strncpy(challenger, buf + 7, MAX_USERNAME_LEN - 1);
                challenger[MAX_USERNAME_LEN - 1] = '\0';
                
                int challenger_idx = find_client_by_username(challenger);
                
                if (challenger_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Joueur introuvable.\n");
                } else if (clients[i].challenged_by != challenger_idx) {
                    send_line(clients[i].socket_fd, "MSG Ce joueur ne vous a pas défié.\n");
                } else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG %s a refusé votre défi.\n", clients[i].username);
                    send_line(clients[challenger_idx].socket_fd, msg);
                    
                    clients[i].challenged_by = -1;  // Réinitialiser le défi
                    send_line(clients[i].socket_fd, "MSG Défi refusé.\n");
                }
            }
            // Commandes de jeu (MOVE, DRAW) pour les clients en partie
            else if (clients[i].status == CLIENT_IN_GAME) {
                Game* g = find_game_for_client(i);
                if (g == NULL) continue;
                
                int player_id = clients[i].player_id;
                int opponent_idx = clients[i].opponent_index;
                
                // Commande QUIT - Abandonner la partie
                if (!strcmp(buf, "QUIT")) {
                    // Le joueur abandonne, l'adversaire gagne
                    int winner = 1 - player_id;
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG %s a abandonné. Vous gagnez!\n", clients[i].username);
                    send_line(clients[opponent_idx].socket_fd, msg);
                    
                    snprintf(msg, sizeof(msg), "END winner %d\n", winner);
                    send_line(clients[opponent_idx].socket_fd, msg);
                    
                    send_line(clients[i].socket_fd, "MSG Vous avez abandonné.\n");
                    send_line(clients[i].socket_fd, "END forfeit\n");
                    
                    // Réinitialiser les statuts
                    clients[g->client_indices[0]].status = CLIENT_WAITING;
                    clients[g->client_indices[1]].status = CLIENT_WAITING;
                    g->active = 0;
                    
                    printf("%s a abandonné contre %s\n", clients[i].username, clients[opponent_idx].username);
                    continue;
                }
                
                // Vérifier que c'est bien le tour du joueur
                if (g->current_player != player_id) {
                    send_line(clients[i].socket_fd, "MSG Ce n'est pas votre tour.\n");
                    continue;
                }
                
                // Traitement d'un coup
                if (!strncmp(buf, "MOVE ", 5)) {
                    int pit = atoi(buf + 5);
                    
                    // Copier l'état du jeu dans les variables globales
                    memcpy(board, g->board, 12);
                    
                    int last = apply_move_from_pit(player_id, pit);
                    
                    if (last == -2) {
                        send_line(clients[i].socket_fd, "MSG Coup invalide.\n");
                        send_game_state(g, i);  // Renvoyer l'état seulement au joueur
                        continue;
                    }
                    
                    // Mettre à jour l'état du jeu
                    memcpy(g->board, board, 12);
                    
                    // Informer l'adversaire
                    char notify[128];
                    snprintf(notify, sizeof(notify), "MSG %s a déplacé les graines de la case %d.\n", 
                             clients[i].username, pit);
                    send_line(clients[opponent_idx].socket_fd, notify);
                    
                    // Capturer les graines
                    char gained = collect_seeds((char)player_id, (char)last);
                    g->scores[player_id] += gained;
                    
                    // Vérifier fin de partie
                    memcpy(board, g->board, 12);
                    memcpy(scores, g->scores, 2);
                    
                    if (is_game_over(CONTINUE)) {
                        collect_remaining_seeds(CONTINUE);
                        memcpy(g->scores, scores, 2);
                        memcpy(g->board, board, 12);
                        broadcast_game_state(g);
                        
                        if (g->scores[0] == g->scores[1]) {
                            send_line(clients[g->client_indices[0]].socket_fd, "END draw\n");
                            send_line(clients[g->client_indices[1]].socket_fd, "END draw\n");
                        } else {
                            int w = (g->scores[0] > g->scores[1]) ? 0 : 1;
                            char msg[32];
                            snprintf(msg, sizeof(msg), "END winner %d\n", w);
                            send_line(clients[g->client_indices[0]].socket_fd, msg);
                            send_line(clients[g->client_indices[1]].socket_fd, msg);
                        }
                        
                        // Réinitialiser les statuts
                        clients[g->client_indices[0]].status = CLIENT_WAITING;
                        clients[g->client_indices[1]].status = CLIENT_WAITING;
                        g->active = 0;
                        continue;
                    }
                    
                    // Changement de joueur
                    g->current_player = 1 - g->current_player;
                    broadcast_game_state(g);
                }
                // Traitement d'une demande d'égalité
                else if (!strcmp(buf, "DRAW")) {
                    send_line(clients[opponent_idx].socket_fd, "ASKDRAW\n");
                    
                    char ans[16];
                    if (recv_line(clients[opponent_idx].socket_fd, ans, sizeof(ans)) > 0) {
                        if (!strcmp(ans, "YES")) {
                            memcpy(board, g->board, 12);
                            memcpy(scores, g->scores, 2);
                            collect_remaining_seeds(DRAW);
                            
                            send_line(clients[g->client_indices[0]].socket_fd, "MSG Égalité acceptée.\n");
                            send_line(clients[g->client_indices[1]].socket_fd, "MSG Égalité acceptée.\n");
                            send_line(clients[g->client_indices[0]].socket_fd, "END draw\n");
                            send_line(clients[g->client_indices[1]].socket_fd, "END draw\n");
                            
                            clients[g->client_indices[0]].status = CLIENT_WAITING;
                            clients[g->client_indices[1]].status = CLIENT_WAITING;
                            g->active = 0;
                        } else {
                            send_line(clients[i].socket_fd, "MSG Égalité refusée.\n");
                            send_line(clients[opponent_idx].socket_fd, "MSG Égalité refusée par l'adversaire.\n");
                            broadcast_game_state(g);
                        }
                    }
                }
            }
        }
    }
    
    // Nettoyage
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd > 0) {
            close(clients[i].socket_fd);
        }
    }
    close(srv);
    
    return 0;
}
