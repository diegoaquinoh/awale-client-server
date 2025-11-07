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
    int spectator_indices[MAX_SPECTATORS];  // Indices des spectateurs
    int num_spectators;
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
 * Valider un nom d'utilisateur
 * Doit avoir au moins 2 caractères et seulement des lettres, chiffres, _ ou -
 */
static int is_valid_username(const char* username) {
    if (!username || strlen(username) < 2) {
        return 0;  // Trop court
    }
    
    if (strlen(username) >= MAX_USERNAME_LEN) {
        return 0;  // Trop long
    }
    
    // Vérifier que tous les caractères sont alphanumériques, _ ou -
    for (const char* p = username; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || 
              (*p >= 'A' && *p <= 'Z') || 
              (*p >= '0' && *p <= '9') || 
              *p == '_' || *p == '-')) {
            return 0;  // Caractère invalide
        }
    }
    
    return 1;  // Valide
}

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
    g->num_spectators = 0;
    for (int i = 0; i < MAX_SPECTATORS; i++) {
        g->spectator_indices[i] = -1;
    }
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
    
    // Envoyer aussi aux spectateurs
    for (int i = 0; i < g->num_spectators; i++) {
        int spec_idx = g->spectator_indices[i];
        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
            send_line(clients[spec_idx].socket_fd, line);
        }
    }
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

/**
 * Envoie la liste des parties en cours à un client
 */
static void send_games_list(int client_idx) {
    char msg[1024] = "GAMESLIST";
    int has_games = 0;
    
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        if (games[i].active) {
            has_games = 1;
            char game_info[128];
            snprintf(game_info, sizeof(game_info), " %d:%s_vs_%s", i,
                     clients[games[i].client_indices[0]].username,
                     clients[games[i].client_indices[1]].username);
            strcat(msg, game_info);
        }
    }
    
    strcat(msg, "\n");
    send_line(clients[client_idx].socket_fd, msg);
    
    if (!has_games) {
        send_line(clients[client_idx].socket_fd, "MSG Aucune partie en cours.\n");
    }
}

/**
 * Termine une partie et notifie les spectateurs
 */
static void end_game(Game* g, const char* end_message) {
    // Notifier les spectateurs
    for (int i = 0; i < g->num_spectators; i++) {
        int spec_idx = g->spectator_indices[i];
        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
            send_line(clients[spec_idx].socket_fd, "MSG La partie que vous regardiez est terminée.\n");
            send_line(clients[spec_idx].socket_fd, end_message);
            clients[spec_idx].status = CLIENT_WAITING;
            clients[spec_idx].watching_game = -1;
        }
    }
    
    // Réinitialiser les statuts des joueurs
    clients[g->client_indices[0]].status = CLIENT_WAITING;
    clients[g->client_indices[1]].status = CLIENT_WAITING;
    
    // Désactiver la partie
    g->active = 0;
    g->num_spectators = 0;
}

int main() {
    srand(time(NULL));
    
    // Initialisation des structures
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = -1;
        clients[i].status = CLIENT_CONNECTED;
        clients[i].opponent_index = -1;
        clients[i].challenged_by = -1;
        clients[i].watching_game = -1;
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
                            
                            // Valider le format du username
                            if (!is_valid_username(username)) {
                                send_line(new_fd, "MSG Username invalide. Il doit contenir au moins 2 caractères alphanumériques, _ ou -. Déconnexion.\n");
                                close(new_fd);
                                printf("Connexion refusée: username '%s' invalide (format)\n", username);
                                continue;
                            }
                            
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
                            clients[num_clients].bio_lines = 0;  // Initialiser la bio vide
                            printf("Client connecté: %s\n", clients[num_clients].username);
                            
                            char welcome[128];
                            snprintf(welcome, sizeof(welcome), "MSG Bienvenue %s! Tapez '/list' pour voir les joueurs disponibles.\n", clients[num_clients].username);
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
            if (recv_line(clients[i].socket_fd, buf, sizeof(buf)) < 0) {
                // Déconnexion
                printf("Client déconnecté: %s\n", clients[i].username);
                
                // Si le client était en partie, l'adversaire gagne automatiquement
                if (clients[i].status == CLIENT_IN_GAME) {
                    Game* g = find_game_for_client(i);
                    if (g) {
                        int opponent_idx = clients[i].opponent_index;
                        if (opponent_idx >= 0 && clients[opponent_idx].socket_fd > 0) {
                            char end_msg[200];
                            snprintf(end_msg, sizeof(end_msg), 
                                    "END %s s'est déconnecté. Vous gagnez par forfait!\n",
                                    clients[i].username);
                            send_line(clients[opponent_idx].socket_fd, end_msg);
                            clients[opponent_idx].status = CLIENT_WAITING;
                            clients[opponent_idx].opponent_index = -1;
                        }
                        
                        // Notifier les spectateurs
                        char spec_msg[200];
                        snprintf(spec_msg, sizeof(spec_msg), 
                                "END %s s'est déconnecté. %s gagne par forfait!\n",
                                clients[i].username, 
                                opponent_idx >= 0 ? clients[opponent_idx].username : "Adversaire");
                        for (int j = 0; j < g->num_spectators; j++) {
                            int spec_idx = g->spectator_indices[j];
                            if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                                send_line(clients[spec_idx].socket_fd, spec_msg);
                                clients[spec_idx].status = CLIENT_WAITING;
                                clients[spec_idx].watching_game = -1;
                            }
                        }
                        
                        // Désactiver la partie
                        g->active = 0;
                        g->num_spectators = 0;
                    }
                }
                // Si le client était spectateur, le retirer de la liste
                else if (clients[i].status == CLIENT_SPECTATING) {
                    Game* g = &games[clients[i].watching_game];
                    for (int j = 0; j < g->num_spectators; j++) {
                        if (g->spectator_indices[j] == i) {
                            for (int k = j; k < g->num_spectators - 1; k++) {
                                g->spectator_indices[k] = g->spectator_indices[k + 1];
                            }
                            g->num_spectators--;
                            break;
                        }
                    }
                }
                
                close(clients[i].socket_fd);
                clients[i].socket_fd = -1;
                clients[i].status = CLIENT_WAITING;
                clients[i].opponent_index = -1;
                clients[i].challenged_by = -1;
                clients[i].watching_game = -1;
                continue;
            }
            
            // Si le client est en train d'éditer sa bio
            if (clients[i].status == CLIENT_EDITING_BIO) {
                // Ligne vide = fin de la bio
                if (strlen(buf) == 0) {
                    clients[i].status = CLIENT_WAITING;
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG Bio enregistrée (%d ligne(s)).\n", clients[i].bio_lines);
                    send_line(clients[i].socket_fd, msg);
                    printf("[%s] a défini sa bio (%d lignes)\n", clients[i].username, clients[i].bio_lines);
                    continue;
                }
                
                // Ajouter la ligne si on n'a pas atteint la limite
                if (clients[i].bio_lines < MAX_BIO_LINES) {
                    strncpy(clients[i].bio[clients[i].bio_lines], buf, MAX_BIO_LINE_LEN - 1);
                    clients[i].bio[clients[i].bio_lines][MAX_BIO_LINE_LEN - 1] = '\0';
                    clients[i].bio_lines++;
                    
                    if (clients[i].bio_lines < MAX_BIO_LINES) {
                        char prompt[64];
                        snprintf(prompt, sizeof(prompt), "MSG Ligne %d: \n", clients[i].bio_lines + 1);
                        send_line(clients[i].socket_fd, prompt);
                    } else {
                        // Limite atteinte, terminer automatiquement
                        clients[i].status = CLIENT_WAITING;
                        char msg[128];
                        snprintf(msg, sizeof(msg), "MSG Bio enregistrée (%d lignes - limite atteinte).\n", clients[i].bio_lines);
                        send_line(clients[i].socket_fd, msg);
                        printf("[%s] a défini sa bio (%d lignes)\n", clients[i].username, clients[i].bio_lines);
                    }
                }
                continue;
            }
            
            // Si le client est spectateur, il ne peut que faire stopwatch ou CHAT
            if (clients[i].status == CLIENT_SPECTATING) {
                if (!strcmp(buf, "STOPWATCH")) {
                    Game* g = &games[clients[i].watching_game];
                    
                    // Retirer le spectateur
                    for (int j = 0; j < g->num_spectators; j++) {
                        if (g->spectator_indices[j] == i) {
                            // Décaler les spectateurs suivants
                            for (int k = j; k < g->num_spectators - 1; k++) {
                                g->spectator_indices[k] = g->spectator_indices[k + 1];
                            }
                            g->num_spectators--;
                            break;
                        }
                    }
                    
                    clients[i].status = CLIENT_WAITING;
                    clients[i].watching_game = -1;
                    
                    send_line(clients[i].socket_fd, "MSG Vous avez arrêté de regarder la partie.\n");
                    printf("%s a arrêté de regarder\n", clients[i].username);
                } else if (!strncmp(buf, "CHAT ", 5)) {
                    // Les spectateurs peuvent envoyer des messages dans le chat de la partie
                    char* message = buf + 5;
                    Game* g = &games[clients[i].watching_game];
                    char chat_msg[512];
                    snprintf(chat_msg, sizeof(chat_msg), "CHAT [Spectateur %s]: %s\n", 
                             clients[i].username, message);
                    
                    // Envoyer aux joueurs
                    for (int j = 0; j < 2; j++) {
                        int player_idx = g->client_indices[j];
                        if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                            send_line(clients[player_idx].socket_fd, chat_msg);
                        }
                    }
                    
                    // Envoyer aux autres spectateurs (SAUF l'expéditeur)
                    for (int j = 0; j < g->num_spectators; j++) {
                        int spec_idx = g->spectator_indices[j];
                        if (spec_idx >= 0 && spec_idx != i && clients[spec_idx].socket_fd > 0) {
                            send_line(clients[spec_idx].socket_fd, chat_msg);
                        }
                    }
                } else {
                    send_line(clients[i].socket_fd, "MSG Vous êtes en mode spectateur. Tapez '/stopwatch' pour quitter ou envoyez un message.\n");
                }
                continue;
            }
            
            // Commande LIST - Demander la liste des utilisateurs
            if (!strcmp(buf, "LIST")) {
                send_online_users(i);
                printf("[%s] a demandé la liste des joueurs\n", clients[i].username);
            }
            // Commande GAMES - Demander la liste des parties en cours
            else if (!strcmp(buf, "GAMES")) {
                send_games_list(i);
                printf("[%s] a demandé la liste des parties\n", clients[i].username);
            }
            // Commande BOARD - Afficher le plateau (pour joueur ou spectateur en partie)
            else if (!strcmp(buf, "BOARD")) {
                if (clients[i].status == CLIENT_IN_GAME) {
                    Game* g = find_game_for_client(i);
                    if (g) {
                        send_game_state(g, i);
                        printf("[%s] a demandé le plateau (en partie)\n", clients[i].username);
                    }
                } else if (clients[i].status == CLIENT_SPECTATING) {
                    Game* g = &games[clients[i].watching_game];
                    send_game_state(g, i);
                    printf("[%s] a demandé le plateau (spectateur)\n", clients[i].username);
                } else {
                    send_line(clients[i].socket_fd, "MSG Vous n'êtes pas en partie.\n");
                }
            }
            // Commande BIO - Définir sa bio (mode édition interactive)
            else if (!strcmp(buf, "BIO")) {
                if (clients[i].status != CLIENT_WAITING) {
                    send_line(clients[i].socket_fd, "MSG Vous ne pouvez éditer votre bio que depuis le lobby.\n");
                } else {
                    clients[i].status = CLIENT_EDITING_BIO;
                    clients[i].bio_lines = 0;
                    send_line(clients[i].socket_fd, "MSG Entrez votre bio (max 10 lignes, ligne vide pour terminer):\n");
                    send_line(clients[i].socket_fd, "MSG Ligne 1: \n");
                    printf("[%s] commence à éditer sa bio\n", clients[i].username);
                }
            }
            // Commande WHOIS - Afficher la bio d'un joueur
            else if (!strncmp(buf, "WHOIS ", 6)) {
                char target[MAX_USERNAME_LEN];
                strncpy(target, buf + 6, MAX_USERNAME_LEN - 1);
                target[MAX_USERNAME_LEN - 1] = '\0';
                
                int target_idx = find_client_by_username(target);
                
                if (target_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Utilisateur introuvable.\n");
                } else {
                    char response[2048];
                    int offset = 0;
                    
                    offset += snprintf(response + offset, sizeof(response) - offset,
                                      "BIO\n=== Bio de %s ===\n", clients[target_idx].username);
                    
                    if (clients[target_idx].bio_lines == 0) {
                        offset += snprintf(response + offset, sizeof(response) - offset,
                                         "(Aucune bio définie)\n");
                    } else {
                        for (int j = 0; j < clients[target_idx].bio_lines; j++) {
                            offset += snprintf(response + offset, sizeof(response) - offset,
                                             "%s\n", clients[target_idx].bio[j]);
                        }
                    }
                    
                    offset += snprintf(response + offset, sizeof(response) - offset,
                                      "==================\n");
                    
                    send_line(clients[i].socket_fd, response);
                    printf("[%s] a consulté la bio de [%s]\n", clients[i].username, target);
                }
            }
            // Commande WATCH - Regarder une partie
            else if (!strncmp(buf, "WATCH ", 6)) {
                int game_id = atoi(buf + 6);
                
                if (game_id < 0 || game_id >= MAX_CLIENTS / 2 || !games[game_id].active) {
                    send_line(clients[i].socket_fd, "MSG Partie introuvable.\n");
                } else if (games[game_id].num_spectators >= MAX_SPECTATORS) {
                    send_line(clients[i].socket_fd, "MSG Partie pleine (trop de spectateurs).\n");
                } else {
                    // Ajouter le spectateur
                    games[game_id].spectator_indices[games[game_id].num_spectators] = i;
                    games[game_id].num_spectators++;
                    
                    clients[i].status = CLIENT_SPECTATING;
                    clients[i].watching_game = game_id;
                    
                    char msg[200];
                    snprintf(msg, sizeof(msg), "MSG Vous regardez la partie entre %s et %s.\n",
                             clients[games[game_id].client_indices[0]].username,
                             clients[games[game_id].client_indices[1]].username);
                    send_line(clients[i].socket_fd, msg);
                    
                    // Envoyer l'état actuel de la partie
                    send_game_state(&games[game_id], i);
                    
                    printf("%s regarde la partie %d\n", clients[i].username, game_id);
                }
            }
            // Commande CHAT - Envoyer un message à un joueur ou à tous (broadcast)
            else if (!strncmp(buf, "CHAT ", 5)) {
                char* message = buf + 5;
                
                // Vérifier si c'est un message privé (format: @username message) ou broadcast (format: message)
                if (message[0] == '@') {
                    // Message privé
                    char* space = strchr(message + 1, ' ');
                    if (space) {
                        *space = '\0';
                        char* target_username = message + 1;
                        char* msg_content = space + 1;
                        
                        int target_idx = find_client_by_username(target_username);
                        
                        if (target_idx == -1) {
                            send_line(clients[i].socket_fd, "MSG Utilisateur introuvable.\n");
                        } else if (target_idx == i) {
                            send_line(clients[i].socket_fd, "MSG Vous ne pouvez pas vous envoyer un message à vous-même.\n");
                        } else {
                            // Envoyer le message privé au destinataire
                            char chat_msg[512];
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT [Privé de %s]: %s\n", 
                                     clients[i].username, msg_content);
                            send_line(clients[target_idx].socket_fd, chat_msg);
                            
                            // NE PAS envoyer de confirmation à l'expéditeur (éviter duplication)
                        }
                    } else {
                        send_line(clients[i].socket_fd, "MSG Format invalide. Utilisez: chat @username message\n");
                    }
                } else {
                    // Message broadcast selon le contexte
                    char chat_msg[512];
                    
                    if (clients[i].status == CLIENT_IN_GAME) {
                        // En partie : envoyer à l'adversaire et aux spectateurs
                        Game* g = find_game_for_client(i);
                        if (g) {
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT [%s]: %s\n", 
                                     clients[i].username, message);
                            
                            // Envoyer à l'adversaire
                            int opponent_idx = clients[i].opponent_index;
                            if (opponent_idx >= 0 && clients[opponent_idx].socket_fd > 0) {
                                send_line(clients[opponent_idx].socket_fd, chat_msg);
                            }
                            
                            // Envoyer aux spectateurs
                            for (int j = 0; j < g->num_spectators; j++) {
                                int spec_idx = g->spectator_indices[j];
                                if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                                    send_line(clients[spec_idx].socket_fd, chat_msg);
                                }
                            }
                        }
                    } else if (clients[i].status == CLIENT_SPECTATING) {
                        // En tant que spectateur : envoyer aux joueurs et autres spectateurs
                        Game* g = &games[clients[i].watching_game];
                        snprintf(chat_msg, sizeof(chat_msg), "CHAT [Spectateur %s]: %s\n", 
                                 clients[i].username, message);
                        
                        // Envoyer aux joueurs
                        for (int j = 0; j < 2; j++) {
                            int player_idx = g->client_indices[j];
                            if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                                send_line(clients[player_idx].socket_fd, chat_msg);
                            }
                        }
                        
                        // Envoyer aux autres spectateurs (SAUF l'expéditeur)
                        for (int j = 0; j < g->num_spectators; j++) {
                            int spec_idx = g->spectator_indices[j];
                            if (spec_idx >= 0 && spec_idx != i && clients[spec_idx].socket_fd > 0) {
                                send_line(clients[spec_idx].socket_fd, chat_msg);
                            }
                        }
                    } else {
                        // Hors partie : broadcast à tous les joueurs en ligne (SAUF l'expéditeur)
                        snprintf(chat_msg, sizeof(chat_msg), "CHAT [Global - %s]: %s\n", 
                                 clients[i].username, message);
                        
                        for (int j = 0; j < num_clients; j++) {
                            if (j != i && clients[j].socket_fd > 0 && clients[j].status == CLIENT_WAITING) {
                                send_line(clients[j].socket_fd, chat_msg);
                            }
                        }
                    }
                }
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
                    printf("[%s] a défié [%s]\n", clients[i].username, target);
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
                    
                    printf("[%s] a accepté le défi de [%s] - Partie %d commencée (P1: %s, P2: %s)\n",
                           clients[i].username, challenger,
                           game_idx,
                           clients[games[game_idx].client_indices[0]].username,
                           clients[games[game_idx].client_indices[1]].username);
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
                    
                    printf("[%s] a refusé le défi de [%s]\n", clients[i].username, challenger);
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
                    
                    // Terminer la partie
                    char end_msg[64];
                    snprintf(end_msg, sizeof(end_msg), "END winner %d\n", winner);
                    end_game(g, end_msg);
                    
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
                    
                    printf("[%s] joue le pit %d\n", clients[i].username, pit);
                    
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
                    
                    // Informer l'adversaire et les spectateurs
                    char notify[128];
                    snprintf(notify, sizeof(notify), "MSG %s a déplacé les graines de la case %d.\n", 
                             clients[i].username, pit);
                    send_line(clients[opponent_idx].socket_fd, notify);
                    
                    // Envoyer aussi aux spectateurs
                    for (int j = 0; j < g->num_spectators; j++) {
                        int spec_idx = g->spectator_indices[j];
                        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                            send_line(clients[spec_idx].socket_fd, notify);
                        }
                    }
                    
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
                        
                        char end_msg[32];
                        if (g->scores[0] == g->scores[1]) {
                            send_line(clients[g->client_indices[0]].socket_fd, "END draw\n");
                            send_line(clients[g->client_indices[1]].socket_fd, "END draw\n");
                            strcpy(end_msg, "END draw\n");
                        } else {
                            int w = (g->scores[0] > g->scores[1]) ? 0 : 1;
                            snprintf(end_msg, sizeof(end_msg), "END winner %d\n", w);
                            send_line(clients[g->client_indices[0]].socket_fd, end_msg);
                            send_line(clients[g->client_indices[1]].socket_fd, end_msg);
                        }
                        
                        // Terminer la partie
                        end_game(g, end_msg);
                        continue;
                    }
                    
                    // Changement de joueur
                    g->current_player = 1 - g->current_player;
                    broadcast_game_state(g);
                }
                // Traitement d'une demande d'égalité
                else if (!strcmp(buf, "DRAW")) {
                    printf("[%s] propose l'égalité à [%s]\n", 
                           clients[i].username, clients[opponent_idx].username);
                    
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
                            
                            printf("Égalité acceptée entre [%s] et [%s]\n",
                                   clients[g->client_indices[0]].username,
                                   clients[g->client_indices[1]].username);
                            
                            // Terminer la partie
                            end_game(g, "END draw\n");
                        } else {
                            send_line(clients[i].socket_fd, "MSG Égalité refusée.\n");
                            send_line(clients[opponent_idx].socket_fd, "MSG Égalité refusée par l'adversaire.\n");
                            broadcast_game_state(g);
                            
                            printf("[%s] a refusé l'égalité proposée par [%s]\n",
                                   clients[opponent_idx].username, clients[i].username);
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
