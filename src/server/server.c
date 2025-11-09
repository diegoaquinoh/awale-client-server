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
#define MAX_MOVES 200

typedef struct {
    int player;      
    int pit;         
    int seeds_captured;  
} Move;

typedef struct {
    int client_indices[2];  
    int spectator_indices[MAX_SPECTATORS];  
    int num_spectators;
    char board[12];
    char scores[2];
    char current_player;
    char active;
    int private_mode;  
    char player_names[2][MAX_USERNAME_LEN];  
    Move moves[MAX_MOVES];  
    int num_moves;  
    time_t start_time;  
    int ending;  
    char end_result[128];  
    int responses_received;  
} Game;

Client clients[MAX_CLIENTS];
Game games[MAX_CLIENTS / 2];
int num_clients = 0;

static int is_valid_username(const char* username) {
    if (!username || strlen(username) < 2) {
        return 0;  
    }
    
    if (strlen(username) >= MAX_USERNAME_LEN) {
        return 0;  
    }
    
    for (const char* p = username; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || 
              (*p >= 'A' && *p <= 'Z') || 
              (*p >= '0' && *p <= '9') || 
              *p == '_' || *p == '-')) {
            return 0;  
        }
    }
    
    return 1;  
}

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
static void send_line(int fd, const char* s) {
    send(fd, s, strlen(s), 0);
}

static void init_game_state(Game* g) {
    for (int i = 0; i < 12; i++) {
        g->board[i] = 4;
    }
    g->scores[0] = 0;
    g->scores[1] = 0;
    g->active = 1;
    g->num_spectators = 0;
    g->private_mode = 0;  
    g->num_moves = 0;  
    g->start_time = time(NULL);  
    g->ending = 0;  
    g->responses_received = 0;  
    for (int i = 0; i < MAX_SPECTATORS; i++) {
        g->spectator_indices[i] = -1;
    }
}

static void save_game(Game* g, const char* result) {
    system("mkdir -p saved_games");
    
    char filename[256];
    struct tm* timeinfo = localtime(&g->start_time);
    snprintf(filename, sizeof(filename), "saved_games/game_%04d%02d%02d_%02d%02d%02d_%s_vs_%s.txt",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             g->player_names[0], g->player_names[1]);
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Erreur: impossible de sauvegarder la partie dans %s\n", filename);
        return;
    }
    
    fprintf(f, "=== PARTIE AWALE ===\n");
    fprintf(f, "Date: %s", ctime(&g->start_time));
    fprintf(f, "Joueur 1 (P1): %s\n", g->player_names[0]);
    fprintf(f, "Joueur 2 (P2): %s\n", g->player_names[1]);
    fprintf(f, "Résultat: %s\n", result);
    fprintf(f, "Score final: %s=%d, %s=%d\n", 
            g->player_names[0], g->scores[0], 
            g->player_names[1], g->scores[1]);
    fprintf(f, "\n=== HISTORIQUE DES COUPS (%d coups) ===\n", g->num_moves);
    
    for (int i = 0; i < g->num_moves; i++) {
        fprintf(f, "Coup %d: %s joue pit %d (capture %d graines)\n",
                i + 1,
                g->player_names[g->moves[i].player],
                g->moves[i].pit,
                g->moves[i].seeds_captured);
    }
    
    fclose(f);
    printf("Partie sauvegardée dans: %s\n", filename);
}

static int find_client_by_socket(int fd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd == fd) {
            return i;
        }
    }
    return -1;
}

static int find_client_by_username(const char* username) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd > 0 && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

static int is_friend(int client_idx, const char* username) {
    for (int i = 0; i < clients[client_idx].num_friends; i++) {
        if (strcmp(clients[client_idx].friends[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

static int add_friend(int client_idx, const char* username) {
    if (clients[client_idx].num_friends >= MAX_FRIENDS) {
        return 0;  
    }
    
    if (is_friend(client_idx, username)) {
        return -1;  
    }
    
    strcpy(clients[client_idx].friends[clients[client_idx].num_friends], username);
    clients[client_idx].num_friends++;
    return 1;  
}

static int remove_friend(int client_idx, const char* username) {
    for (int i = 0; i < clients[client_idx].num_friends; i++) {
        if (strcmp(clients[client_idx].friends[i], username) == 0) {
            for (int j = i; j < clients[client_idx].num_friends - 1; j++) {
                strcpy(clients[client_idx].friends[j], clients[client_idx].friends[j + 1]);
            }
            clients[client_idx].num_friends--;
            return 1;  
        }
    }
    return 0;  
}

static int has_friend_request(int client_idx, const char* username) {
    for (int i = 0; i < clients[client_idx].num_friend_requests; i++) {
        if (strcmp(clients[client_idx].friend_requests[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

static int add_friend_request(int client_idx, const char* username) {
    if (clients[client_idx].num_friend_requests >= MAX_FRIENDS) {
        return 0;  
    }
    
    if (has_friend_request(client_idx, username)) {
        return -1;  
    }
    
    strcpy(clients[client_idx].friend_requests[clients[client_idx].num_friend_requests], username);
    clients[client_idx].num_friend_requests++;
    return 1;  
}

static int remove_friend_request(int client_idx, const char* username) {
    for (int i = 0; i < clients[client_idx].num_friend_requests; i++) {
        if (strcmp(clients[client_idx].friend_requests[i], username) == 0) {
            for (int j = i; j < clients[client_idx].num_friend_requests - 1; j++) {
                strcpy(clients[client_idx].friend_requests[j], clients[client_idx].friend_requests[j + 1]);
            }
            clients[client_idx].num_friend_requests--;
            return 1;  
        }
    }
    return 0;  
}

static int can_spectate(int spectator_idx, Game* g) {
    if (!g->private_mode) {
        return 1;
    }
    
    for (int i = 0; i < 2; i++) {
        int player_idx = g->client_indices[i];
        if (player_idx >= 0 && clients[player_idx].private_mode) {
            if (is_friend(player_idx, clients[spectator_idx].username)) {
                return 1;  
            }
        }
    }
    
    return 0;  
}

static void update_elo(Game* g, int winner) {
    int p0_idx = g->client_indices[0];
    int p1_idx = g->client_indices[1];
    
    if (p0_idx < 0 || p1_idx < 0) return;
    
    if (is_friend(p0_idx, clients[p1_idx].username)) {
        return;  
    }
    
    if (winner == 0) {
        clients[p0_idx].elo_score++;
        if (clients[p1_idx].elo_score > 0) {
            clients[p1_idx].elo_score--;
        }
    } else if (winner == 1) {
        clients[p1_idx].elo_score++;
        if (clients[p0_idx].elo_score > 0) {
            clients[p0_idx].elo_score--;
        }
    }
}

static void send_online_users(int client_idx) {
    int available[MAX_CLIENTS];
    int count = 0;
    
    for (int i = 0; i < num_clients; i++) {
        if (i != client_idx && 
            clients[i].socket_fd > 0 && 
            clients[i].status != CLIENT_IN_GAME) {
            available[count++] = i;
        }
    }
    
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (clients[available[j]].elo_score < clients[available[j + 1]].elo_score) {
                int tmp = available[j];
                available[j] = available[j + 1];
                available[j + 1] = tmp;
            }
        }
    }
    
    char msg[1024] = "USERLIST";
    for (int i = 0; i < count; i++) {
        char entry[128];
        snprintf(entry, sizeof(entry), " %s(%d)", 
                 clients[available[i]].username, 
                 clients[available[i]].elo_score);
        strcat(msg, entry);
    }
    
    strcat(msg, "\n");
    send_line(clients[client_idx].socket_fd, msg);
}

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

static int find_game_index_for_client(int client_idx) {
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        if (games[i].active && 
            (games[i].client_indices[0] == client_idx || 
             games[i].client_indices[1] == client_idx)) {
            return i;
        }
    }
    return -1;
}

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
    
    for (int i = 0; i < g->num_spectators; i++) {
        int spec_idx = g->spectator_indices[i];
        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
            send_line(clients[spec_idx].socket_fd, line);
        }
    }
}

static void send_game_state(Game* g, int client_idx) {
    char line[256];
    
    snprintf(line, sizeof(line),
        "STATE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        g->board[0], g->board[1], g->board[2], g->board[3], g->board[4], g->board[5],
        g->board[6], g->board[7], g->board[8], g->board[9], g->board[10], g->board[11],
        g->scores[0], g->scores[1], g->current_player);
    
    send_line(clients[client_idx].socket_fd, line);
}

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

static void finalize_game_end(Game* g, int game_idx) {
    int save_requested = 0;
    for (int i = 0; i < 2; i++) {
        int player_idx = g->client_indices[i];
        if (player_idx >= 0 && clients[player_idx].save_response == 1) {
            save_requested = 1;
            break;
        }
    }
    
    if (save_requested) {
        save_game(g, g->end_result);
        
        for (int i = 0; i < 2; i++) {
            int player_idx = g->client_indices[i];
            if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                send_line(clients[player_idx].socket_fd, "MSG Partie sauvegardée.\n");
            }
        }
    } else {
        for (int i = 0; i < 2; i++) {
            int player_idx = g->client_indices[i];
            if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                send_line(clients[player_idx].socket_fd, "MSG Partie non sauvegardée.\n");
            }
        }
    }
    
    for (int i = 0; i < g->num_spectators; i++) {
        int spec_idx = g->spectator_indices[i];
        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
            send_line(clients[spec_idx].socket_fd, "MSG La partie que vous regardiez est terminée.\n");
            clients[spec_idx].status = CLIENT_WAITING;
            clients[spec_idx].watching_game = -1;
        }
    }
    
    for (int i = 0; i < 2; i++) {
        int player_idx = g->client_indices[i];
        if (player_idx >= 0) {
            clients[player_idx].save_response = -1;
            clients[player_idx].game_to_save = -1;
        }
    }
    
    g->active = 0;
    g->num_spectators = 0;
    g->ending = 0;
}

static void end_game(Game* g, const char* end_message, int game_idx) {
    int winner = -1;  
    
    if (strstr(end_message, "draw")) {
        snprintf(g->end_result, sizeof(g->end_result), "Égalité");
        winner = -1;
    } else if (strstr(end_message, "winner 0")) {
        snprintf(g->end_result, sizeof(g->end_result), "%s gagne", g->player_names[0]);
        winner = 0;
    } else if (strstr(end_message, "winner 1")) {
        snprintf(g->end_result, sizeof(g->end_result), "%s gagne", g->player_names[1]);
        winner = 1;
    } else {
        snprintf(g->end_result, sizeof(g->end_result), "Partie interrompue");
        winner = -1;  
    }
    
    if (!strstr(end_message, "interrompu") && !strstr(end_message, "forfait") && !strstr(end_message, "déconnecté")) {
        update_elo(g, winner);
    }
    
    int auto_save = 0;
    for (int i = 0; i < 2; i++) {
        int player_idx = g->client_indices[i];
        if (player_idx >= 0 && clients[player_idx].save_mode) {
            auto_save = 1;
            break;
        }
    }
    
    if (auto_save) {
        save_game(g, g->end_result);
        
        for (int i = 0; i < 2; i++) {
            int player_idx = g->client_indices[i];
            if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                send_line(clients[player_idx].socket_fd, "MSG Partie sauvegardée automatiquement.\n");
                clients[player_idx].status = CLIENT_WAITING;
            }
        }
        
        for (int i = 0; i < g->num_spectators; i++) {
            int spec_idx = g->spectator_indices[i];
            if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                send_line(clients[spec_idx].socket_fd, "MSG La partie que vous regardiez est terminée.\n");
                clients[spec_idx].status = CLIENT_WAITING;
                clients[spec_idx].watching_game = -1;
            }
        }
        
        g->active = 0;
        g->num_spectators = 0;
        return;
    }
    
    g->ending = 1;
    g->responses_received = 0;
    
    int players_asked = 0;
    for (int i = 0; i < 2; i++) {
        int player_idx = g->client_indices[i];
        if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
            send_line(clients[player_idx].socket_fd, "ASKSAVE\n");
            clients[player_idx].status = CLIENT_ASKED_SAVE;
            clients[player_idx].save_response = -1;  
            clients[player_idx].game_to_save = game_idx;
            players_asked++;
        }
    }
    
    if (players_asked == 0) {
        for (int i = 0; i < g->num_spectators; i++) {
            int spec_idx = g->spectator_indices[i];
            if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                send_line(clients[spec_idx].socket_fd, "MSG La partie que vous regardiez est terminée.\n");
                send_line(clients[spec_idx].socket_fd, end_message);
                clients[spec_idx].status = CLIENT_WAITING;
                clients[spec_idx].watching_game = -1;
            }
        }
        
        g->active = 0;
        g->num_spectators = 0;
        g->ending = 0;
    }
}

int main() {
    srand(time(NULL));
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = -1;
        clients[i].status = CLIENT_CONNECTED;
        clients[i].opponent_index = -1;
        clients[i].challenged_by = -1;
        clients[i].watching_game = -1;
        clients[i].save_mode = 0;
        clients[i].save_response = -1;
        clients[i].game_to_save = -1;
    }
    
    for (int i = 0; i < MAX_CLIENTS / 2; i++) {
        games[i].active = 0;
        games[i].ending = 0;
    }
    
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(PORT);
    
    if (bind(srv, (struct sockaddr*)&a, sizeof(a)) < 0 || listen(srv, 10) < 0) {
        perror("bind/listen");
        return 1;
    }
    
    printf("Server on %d\n", PORT);
    
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        
        int maxfd = srv;
        
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
        
        if (FD_ISSET(srv, &rfds)) {
            if (num_clients < MAX_CLIENTS) {
                int new_fd = accept(srv, NULL, NULL);
                if (new_fd >= 0) {
                    clients[num_clients].socket_fd = new_fd;
                    clients[num_clients].status = CLIENT_CONNECTED;  
                    clients[num_clients].opponent_index = -1;
                    clients[num_clients].challenged_by = -1;
                    clients[num_clients].watching_game = -1;
                    clients[num_clients].username[0] = '\0';  
                    clients[num_clients].bio_lines = 0;
                    clients[num_clients].num_friends = 0;
                    clients[num_clients].num_friend_requests = 0;
                    clients[num_clients].private_mode = 0;
                    clients[num_clients].save_mode = 0;
                    clients[num_clients].save_response = -1;
                    clients[num_clients].game_to_save = -1;
                    clients[num_clients].elo_score = 100;  
                    
                    send_line(new_fd, "REGISTER\n");
                    
                    printf("Nouvelle connexion acceptée (en attente du username)\n");
                    num_clients++;
                }
            }
        }
        
        for (int i = 0; i < num_clients; i++) {
            if (clients[i].socket_fd <= 0 || !FD_ISSET(clients[i].socket_fd, &rfds)) {
                continue;
            }
            
            char buf[256];
            if (recv_line(clients[i].socket_fd, buf, sizeof(buf)) < 0) {
                if (clients[i].username[0] != '\0') {
                    printf("Client déconnecté: %s\n", clients[i].username);
                } else {
                    printf("Client déconnecté (pas de username)\n");
                }
                
                if (clients[i].status == CLIENT_IN_GAME) {
                    int game_idx = find_game_index_for_client(i);
                    Game* g = (game_idx >= 0) ? &games[game_idx] : NULL;
                    if (g) {
                        int opponent_idx = clients[i].opponent_index;
                        
                        int winner_id = 1 - clients[i].player_id;
                        snprintf(g->end_result, sizeof(g->end_result), "%s gagne par forfait (%s déconnecté)", 
                                g->player_names[winner_id], clients[i].username);
                        
                        if (opponent_idx >= 0 && clients[opponent_idx].socket_fd > 0) {
                            char end_msg[200];
                            snprintf(end_msg, sizeof(end_msg), 
                                    "END %s s'est déconnecté. Vous gagnez par forfait!\n",
                                    clients[i].username);
                            send_line(clients[opponent_idx].socket_fd, end_msg);
                            
                            if (clients[opponent_idx].save_mode || clients[i].save_mode) {
                                save_game(g, g->end_result);
                                send_line(clients[opponent_idx].socket_fd, "MSG Partie sauvegardée automatiquement.\n");
                                clients[opponent_idx].status = CLIENT_WAITING;
                                clients[opponent_idx].opponent_index = -1;
                                g->active = 0;
                                g->num_spectators = 0;
                            } else {
                                g->ending = 1;
                                g->responses_received = 0;
                                send_line(clients[opponent_idx].socket_fd, "ASKSAVE\n");
                                clients[opponent_idx].status = CLIENT_ASKED_SAVE;
                                clients[opponent_idx].save_response = -1;
                                clients[opponent_idx].game_to_save = game_idx;
                                clients[opponent_idx].opponent_index = -1;
                            }
                        } else {
                            if (clients[i].save_mode) {
                                save_game(g, g->end_result);
                            }
                            g->active = 0;
                            g->num_spectators = 0;
                        }
                        
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
                    }
                }
                else if (clients[i].status == CLIENT_ASKED_SAVE) {
                    int game_idx = clients[i].game_to_save;
                    if (game_idx >= 0 && game_idx < MAX_CLIENTS / 2 && games[game_idx].ending) {
                        clients[i].save_response = 0;
                        games[game_idx].responses_received++;
                        
                        int connected_players = 0;
                        for (int j = 0; j < 2; j++) {
                            int player_idx = games[game_idx].client_indices[j];
                            if (player_idx >= 0 && player_idx != i && clients[player_idx].socket_fd > 0) {
                                connected_players++;
                            }
                        }
                        
                        if (games[game_idx].responses_received >= connected_players + 1) {
                            finalize_game_end(&games[game_idx], game_idx);
                        }
                    }
                }
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
            
            if (clients[i].status == CLIENT_CONNECTED) {
                if (strncmp(buf, "USERNAME ", 9) == 0) {
                    char username[MAX_USERNAME_LEN];
                    strncpy(username, buf + 9, MAX_USERNAME_LEN - 1);
                    username[MAX_USERNAME_LEN - 1] = '\0';
                    
                    if (!is_valid_username(username)) {
                        send_line(clients[i].socket_fd, "MSG Username invalide. Il doit contenir au moins 2 caractères alphanumériques, _ ou -. Déconnexion.\n");
                        close(clients[i].socket_fd);
                        clients[i].socket_fd = -1;
                        printf("Connexion refusée: username '%s' invalide (format)\n", username);
                        continue;
                    }
                    
                    int username_taken = 0;
                    for (int j = 0; j < num_clients; j++) {
                        if (j != i && clients[j].socket_fd > 0 && strcmp(clients[j].username, username) == 0) {
                            username_taken = 1;
                            break;
                        }
                    }
                    
                    if (username_taken) {
                        send_line(clients[i].socket_fd, "MSG Username déjà pris. Déconnexion.\n");
                        close(clients[i].socket_fd);
                        clients[i].socket_fd = -1;
                        printf("Connexion refusée: username '%s' déjà pris\n", username);
                        continue;
                    }
                    
                    strcpy(clients[i].username, username);
                    clients[i].status = CLIENT_WAITING;
                    
                    char welcome[128];
                    snprintf(welcome, sizeof(welcome), "MSG Bienvenue %s! Tapez '/list' pour voir les joueurs disponibles.\n", username);
                    send_line(clients[i].socket_fd, welcome);
                    
                    printf("Client connecté: %s\n", username);
                } else {
                    send_line(clients[i].socket_fd, "MSG Veuillez envoyer votre username avec 'USERNAME <nom>'.\n");
                }
                continue;
            }
            
            if (clients[i].status == CLIENT_ASKED_SAVE) {
                if (!strcmp(buf, "YES")) {
                    clients[i].save_response = 1;
                } else {
                    clients[i].save_response = 0;
                }
                
                int game_idx = clients[i].game_to_save;
                if (game_idx >= 0 && game_idx < MAX_CLIENTS / 2 && games[game_idx].ending) {
                    games[game_idx].responses_received++;
                    
                    clients[i].status = CLIENT_WAITING;
                    clients[i].game_to_save = -1;
                    send_line(clients[i].socket_fd, "MSG Réponse enregistrée.\n");
                    
                    int connected_players = 0;
                    for (int j = 0; j < 2; j++) {
                        int player_idx = games[game_idx].client_indices[j];
                        if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                            connected_players++;
                        }
                    }
                    
                    if (games[game_idx].responses_received >= connected_players) {
                        finalize_game_end(&games[game_idx], game_idx);
                    }
                }
                
                continue;
            }
            
            if (clients[i].status == CLIENT_EDITING_BIO) {
                if (strlen(buf) == 0) {
                    clients[i].status = CLIENT_WAITING;
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG Bio enregistrée (%d ligne(s)).\n", clients[i].bio_lines);
                    send_line(clients[i].socket_fd, msg);
                    printf("[%s] a défini sa bio (%d lignes)\n", clients[i].username, clients[i].bio_lines);
                    continue;
                }
                
                if (clients[i].bio_lines < MAX_BIO_LINES) {
                    strncpy(clients[i].bio[clients[i].bio_lines], buf, MAX_BIO_LINE_LEN - 1);
                    clients[i].bio[clients[i].bio_lines][MAX_BIO_LINE_LEN - 1] = '\0';
                    clients[i].bio_lines++;
                    
                    if (clients[i].bio_lines < MAX_BIO_LINES) {
                        char prompt[64];
                        snprintf(prompt, sizeof(prompt), "MSG Ligne %d: \n", clients[i].bio_lines + 1);
                        send_line(clients[i].socket_fd, prompt);
                    } else {
                        clients[i].status = CLIENT_WAITING;
                        char msg[128];
                        snprintf(msg, sizeof(msg), "MSG Bio enregistrée (%d lignes - limite atteinte).\n", clients[i].bio_lines);
                        send_line(clients[i].socket_fd, msg);
                        printf("[%s] a défini sa bio (%d lignes)\n", clients[i].username, clients[i].bio_lines);
                    }
                }
                continue;
            }
            
            if (clients[i].status == CLIENT_SPECTATING) {
                if (!strcmp(buf, "STOPWATCH")) {
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
                    
                    clients[i].status = CLIENT_WAITING;
                    clients[i].watching_game = -1;
                    
                    send_line(clients[i].socket_fd, "MSG Vous avez arrêté de regarder la partie.\n");
                    printf("%s a arrêté de regarder\n", clients[i].username);
                } else if (!strncmp(buf, "CHAT ", 5)) {
                    char* message = buf + 5;
                    Game* g = &games[clients[i].watching_game];
                    char chat_msg[512];
                    snprintf(chat_msg, sizeof(chat_msg), "CHAT [Spectateur %s]: %s\n", 
                             clients[i].username, message);
                    
                    for (int j = 0; j < 2; j++) {
                        int player_idx = g->client_indices[j];
                        if (player_idx >= 0 && clients[player_idx].socket_fd > 0) {
                            send_line(clients[player_idx].socket_fd, chat_msg);
                        }
                    }
                    
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
            
            if (!strcmp(buf, "LIST")) {
                send_online_users(i);
                printf("[%s] a demandé la liste des joueurs\n", clients[i].username);
            }
            else if (!strcmp(buf, "GAMES")) {
                send_games_list(i);
                printf("[%s] a demandé la liste des parties\n", clients[i].username);
            }
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
            else if (!strncmp(buf, "ADDFRIEND ", 10)) {
                char friend_name[MAX_USERNAME_LEN];
                strncpy(friend_name, buf + 10, MAX_USERNAME_LEN - 1);
                friend_name[MAX_USERNAME_LEN - 1] = '\0';
                
                int friend_idx = find_client_by_username(friend_name);
                
                if (friend_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Utilisateur introuvable.\n");
                } else if (friend_idx == i) {
                    send_line(clients[i].socket_fd, "MSG Vous ne pouvez pas vous ajouter vous-même comme ami.\n");
                } else if (is_friend(i, friend_name)) {
                    send_line(clients[i].socket_fd, "MSG Cet utilisateur est déjà votre ami.\n");
                } else {
                    int result = add_friend_request(friend_idx, clients[i].username);
                    if (result == 1) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "MSG Demande d'ami envoyée à %s.\n", friend_name);
                        send_line(clients[i].socket_fd, msg);
                        
                        char notif[200];
                        snprintf(notif, sizeof(notif), "MSG %s vous a envoyé une demande d'ami. Tapez '/acceptfriend %s' pour accepter.\n", 
                                clients[i].username, clients[i].username);
                        send_line(clients[friend_idx].socket_fd, notif);
                        
                        printf("[%s] a envoyé une demande d'ami à [%s]\n", clients[i].username, friend_name);
                    } else if (result == -1) {
                        send_line(clients[i].socket_fd, "MSG Vous avez déjà envoyé une demande d'ami à cet utilisateur.\n");
                    } else {
                        send_line(clients[i].socket_fd, "MSG L'utilisateur a trop de demandes en attente.\n");
                    }
                }
            }
            else if (!strncmp(buf, "ACCEPTFRIEND ", 13)) {
                char friend_name[MAX_USERNAME_LEN];
                strncpy(friend_name, buf + 13, MAX_USERNAME_LEN - 1);
                friend_name[MAX_USERNAME_LEN - 1] = '\0';
                
                if (!has_friend_request(i, friend_name)) {
                    send_line(clients[i].socket_fd, "MSG Vous n'avez pas de demande d'ami de cet utilisateur.\n");
                } else {
                    int friend_idx = find_client_by_username(friend_name);
                    
                    int result1 = add_friend(i, friend_name);
                    int result2 = -1;
                    if (friend_idx != -1) {
                        result2 = add_friend(friend_idx, clients[i].username);
                    }
                    
                    if (result1 == 1 && result2 == 1) {
                        remove_friend_request(i, friend_name);
                        
                        char msg[128];
                        snprintf(msg, sizeof(msg), "MSG Vous êtes maintenant ami avec %s.\n", friend_name);
                        send_line(clients[i].socket_fd, msg);
                        
                        if (friend_idx != -1) {
                            snprintf(msg, sizeof(msg), "MSG %s a accepté votre demande d'ami.\n", clients[i].username);
                            send_line(clients[friend_idx].socket_fd, msg);
                        }
                        
                        printf("[%s] et [%s] sont maintenant amis\n", clients[i].username, friend_name);
                    } else {
                        send_line(clients[i].socket_fd, "MSG Erreur: liste d'amis pleine.\n");
                    }
                }
            }
            else if (!strcmp(buf, "LISTFRIENDREQUESTS")) {
                char line[256];
                
                snprintf(line, sizeof(line), "MSG === Demandes d'amis reçues (%d) ===\n", clients[i].num_friend_requests);
                send_line(clients[i].socket_fd, line);
                
                if (clients[i].num_friend_requests == 0) {
                    send_line(clients[i].socket_fd, "MSG Aucune demande d'ami en attente.\n");
                } else {
                    for (int j = 0; j < clients[i].num_friend_requests; j++) {
                        snprintf(line, sizeof(line), "MSG - %s (tapez '/acceptfriend %s' pour accepter)\n", 
                                clients[i].friend_requests[j], clients[i].friend_requests[j]);
                        send_line(clients[i].socket_fd, line);
                    }
                }
                
                send_line(clients[i].socket_fd, "MSG ==============================\n");
            }
            else if (!strncmp(buf, "REMOVEFRIEND ", 13)) {
                char friend_name[MAX_USERNAME_LEN];
                strncpy(friend_name, buf + 13, MAX_USERNAME_LEN - 1);
                friend_name[MAX_USERNAME_LEN - 1] = '\0';
                
                int result = remove_friend(i, friend_name);
                if (result == 1) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG %s a été retiré de votre liste d'amis.\n", friend_name);
                    send_line(clients[i].socket_fd, msg);
                    printf("[%s] a retiré [%s] de sa liste d'amis\n", clients[i].username, friend_name);
                } else {
                    send_line(clients[i].socket_fd, "MSG Cet utilisateur n'est pas dans votre liste d'amis.\n");
                }
            }
            else if (!strcmp(buf, "LISTFRIENDS")) {
                char line[256];
                
                snprintf(line, sizeof(line), "MSG === Vos amis (%d/%d) ===\n", clients[i].num_friends, MAX_FRIENDS);
                send_line(clients[i].socket_fd, line);
                
                if (clients[i].num_friends == 0) {
                    send_line(clients[i].socket_fd, "MSG Aucun ami dans votre liste.\n");
                } else {
                    for (int j = 0; j < clients[i].num_friends; j++) {
                        snprintf(line, sizeof(line), "MSG - %s\n", clients[i].friends[j]);
                        send_line(clients[i].socket_fd, line);
                    }
                }
                
                send_line(clients[i].socket_fd, "MSG ==================\n");
            }
            else if (!strcmp(buf, "PRIVATE")) {
                clients[i].private_mode = !clients[i].private_mode;
                
                if (clients[i].private_mode) {
                    send_line(clients[i].socket_fd, "MSG Mode privé activé. Seuls vos amis pourront regarder vos parties.\n");
                    printf("[%s] a activé le mode privé\n", clients[i].username);
                } else {
                    send_line(clients[i].socket_fd, "MSG Mode privé désactivé. Tout le monde peut regarder vos parties.\n");
                    printf("[%s] a désactivé le mode privé\n", clients[i].username);
                }
            }
            else if (!strcmp(buf, "SAVE")) {
                clients[i].save_mode = !clients[i].save_mode;
                
                if (clients[i].save_mode) {
                    send_line(clients[i].socket_fd, "MSG Mode sauvegarde activé. Vos parties seront automatiquement sauvegardées.\n");
                    printf("[%s] a activé le mode sauvegarde\n", clients[i].username);
                } else {
                    send_line(clients[i].socket_fd, "MSG Mode sauvegarde désactivé. Vos parties ne seront plus sauvegardées automatiquement.\n");
                    printf("[%s] a désactivé le mode sauvegarde\n", clients[i].username);
                }
            }
            else if (!strcmp(buf, "HISTORY")) {
                FILE* p = popen("ls -1t saved_games/*.txt 2>/dev/null | head -20", "r");
                if (!p) {
                    send_line(clients[i].socket_fd, "MSG Aucune partie sauvegardée.\n");
                } else {
                    char line[512];
                    char response[4096] = "MSG === Parties sauvegardées (max 20) ===\n";
                    int count = 0;
                    
                    while (fgets(line, sizeof(line), p) && count < 20) {
                        line[strcspn(line, "\n")] = 0;
                        
                        char* filename = strrchr(line, '/');
                        if (filename) filename++;
                        else filename = line;
                        
                        char entry[256];
                        snprintf(entry, sizeof(entry), "%d. %s\n", ++count, filename);
                        strcat(response, entry);
                    }
                    
                    if (count == 0) {
                        strcpy(response, "MSG Aucune partie sauvegardée.\n");
                    } else {
                        strcat(response, "Tapez '/replay <numéro>' pour revoir une partie.\n");
                    }
                    
                    pclose(p);
                    send_line(clients[i].socket_fd, response);
                }
            }
            else if (!strncmp(buf, "REPLAY ", 7)) {
                int game_num = atoi(buf + 7);
                
                if (game_num < 1 || game_num > 20) {
                    send_line(clients[i].socket_fd, "MSG Numéro invalide. Tapez '/history' pour voir la liste.\n");
                } else {
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "ls -1t saved_games/*.txt 2>/dev/null | head -20 | sed -n '%dp'", game_num);
                    
                    FILE* p = popen(cmd, "r");
                    if (!p) {
                        send_line(clients[i].socket_fd, "MSG Erreur lors de la lecture.\n");
                    } else {
                        char filename[256];
                        if (fgets(filename, sizeof(filename), p)) {
                            filename[strcspn(filename, "\n")] = 0;
                            pclose(p);
                            
                            FILE* f = fopen(filename, "r");
                            if (!f) {
                                send_line(clients[i].socket_fd, "MSG Impossible d'ouvrir le fichier.\n");
                            } else {
                                char response[8192] = "REPLAY\n";
                                char line[512];
                                
                                while (fgets(line, sizeof(line), f)) {
                                    strcat(response, line);
                                }
                                
                                fclose(f);
                                send_line(clients[i].socket_fd, response);
                                printf("[%s] a consulté la partie: %s\n", clients[i].username, filename);
                            }
                        } else {
                            pclose(p);
                            send_line(clients[i].socket_fd, "MSG Partie introuvable.\n");
                        }
                    }
                }
            }
            else if (!strncmp(buf, "WATCH ", 6)) {
                int game_id = atoi(buf + 6);
                
                if (game_id < 0 || game_id >= MAX_CLIENTS / 2 || !games[game_id].active) {
                    send_line(clients[i].socket_fd, "MSG Partie introuvable.\n");
                } else if (games[game_id].num_spectators >= MAX_SPECTATORS) {
                    send_line(clients[i].socket_fd, "MSG Partie pleine (trop de spectateurs).\n");
                } else if (!can_spectate(i, &games[game_id])) {
                    send_line(clients[i].socket_fd, "MSG Cette partie est en mode privé. Vous devez être ami avec un des joueurs.\n");
                } else {
                    games[game_id].spectator_indices[games[game_id].num_spectators] = i;
                    games[game_id].num_spectators++;
                    
                    clients[i].status = CLIENT_SPECTATING;
                    clients[i].watching_game = game_id;
                    
                    char msg[200];
                    snprintf(msg, sizeof(msg), "MSG Vous regardez la partie entre %s et %s.\n",
                             clients[games[game_id].client_indices[0]].username,
                             clients[games[game_id].client_indices[1]].username);
                    send_line(clients[i].socket_fd, msg);
                    
                    send_game_state(&games[game_id], i);
                    
                    printf("%s regarde la partie %d\n", clients[i].username, game_id);
                }
            }
            else if (!strncmp(buf, "CHAT ", 5)) {
                char* message = buf + 5;
                
                if (message[0] == '@') {
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
                        } else if (clients[target_idx].status == CLIENT_EDITING_BIO) {
                            char wait_msg[256];
                            snprintf(wait_msg, sizeof(wait_msg), 
                                     "MSG %s est en train d'éditer sa bio. Attendez qu'il termine.\n", 
                                     clients[target_idx].username);
                            send_line(clients[i].socket_fd, wait_msg);
                        } else {
                            char chat_msg[512];
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT [Privé de %s]: %s\n", 
                                     clients[i].username, msg_content);
                            send_line(clients[target_idx].socket_fd, chat_msg);
                            
                        }
                    } else {
                        send_line(clients[i].socket_fd, "MSG Format invalide. Utilisez: chat @username message\n");
                    }
                } else {
                    char chat_msg[512];
                    
                    if (clients[i].status == CLIENT_IN_GAME) {
                        Game* g = find_game_for_client(i);
                        if (g) {
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT [%s]: %s\n", 
                                     clients[i].username, message);
                            
                            int opponent_idx = clients[i].opponent_index;
                            if (opponent_idx >= 0 && clients[opponent_idx].socket_fd > 0 
                                && clients[opponent_idx].status != CLIENT_EDITING_BIO) {
                                send_line(clients[opponent_idx].socket_fd, chat_msg);
                            }
                            
                            for (int j = 0; j < g->num_spectators; j++) {
                                int spec_idx = g->spectator_indices[j];
                                if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0
                                    && clients[spec_idx].status != CLIENT_EDITING_BIO) {
                                    send_line(clients[spec_idx].socket_fd, chat_msg);
                                }
                            }
                        }
                    } else if (clients[i].status == CLIENT_SPECTATING) {
                        Game* g = &games[clients[i].watching_game];
                        snprintf(chat_msg, sizeof(chat_msg), "CHAT [Spectateur %s]: %s\n", 
                                 clients[i].username, message);
                        
                        for (int j = 0; j < 2; j++) {
                            int player_idx = g->client_indices[j];
                            if (player_idx >= 0 && clients[player_idx].socket_fd > 0
                                && clients[player_idx].status != CLIENT_EDITING_BIO) {
                                send_line(clients[player_idx].socket_fd, chat_msg);
                            }
                        }
                        
                        for (int j = 0; j < g->num_spectators; j++) {
                            int spec_idx = g->spectator_indices[j];
                            if (spec_idx >= 0 && spec_idx != i && clients[spec_idx].socket_fd > 0
                                && clients[spec_idx].status != CLIENT_EDITING_BIO) {
                                send_line(clients[spec_idx].socket_fd, chat_msg);
                            }
                        }
                    } else {
                        snprintf(chat_msg, sizeof(chat_msg), "CHAT [Global - %s]: %s\n", 
                                 clients[i].username, message);
                        
                        for (int j = 0; j < num_clients; j++) {
                            if (j != i && clients[j].socket_fd > 0 && clients[j].status == CLIENT_WAITING
                                && clients[j].status != CLIENT_EDITING_BIO) {
                                send_line(clients[j].socket_fd, chat_msg);
                            }
                        }
                    }
                }
            }
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
                    clients[target_idx].challenged_by = i;
                    
                    char challenge_msg[128];
                    snprintf(challenge_msg, sizeof(challenge_msg), "CHALLENGED_BY %s\n", clients[i].username);
                    send_line(clients[target_idx].socket_fd, challenge_msg);
                    
                    send_line(clients[i].socket_fd, "MSG Défi envoyé. En attente de réponse...\n");
                    printf("[%s] a défié [%s]\n", clients[i].username, target);
                }
            }
            else if (!strncmp(buf, "ACCEPT ", 7)) {
                char challenger[MAX_USERNAME_LEN];
                strncpy(challenger, buf + 7, MAX_USERNAME_LEN - 1);
                challenger[MAX_USERNAME_LEN - 1] = '\0';
                
                int challenger_idx = find_client_by_username(challenger);
                
                if (challenger_idx == -1) {
                    send_line(clients[i].socket_fd, "MSG Joueur introuvable.\n");
                } else if (clients[i].challenged_by != challenger_idx) {
                    send_line(clients[i].socket_fd, "MSG Ce joueur ne vous a pas défié.\n");
                } else {
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
                    
                    strcpy(games[game_idx].player_names[0], clients[games[game_idx].client_indices[0]].username);
                    strcpy(games[game_idx].player_names[1], clients[games[game_idx].client_indices[1]].username);
                    
                    if (clients[challenger_idx].private_mode || clients[i].private_mode) {
                        games[game_idx].private_mode = 1;
                    }
                    
                    clients[challenger_idx].status = CLIENT_IN_GAME;
                    clients[challenger_idx].opponent_index = i;
                    clients[i].status = CLIENT_IN_GAME;
                    clients[i].opponent_index = challenger_idx;
                    clients[i].challenged_by = -1;  
                    
                    clients[games[game_idx].client_indices[0]].player_id = 0;
                    clients[games[game_idx].client_indices[1]].player_id = 1;
                    
                    send_line(clients[games[game_idx].client_indices[0]].socket_fd, "ROLE 0\n");
                    send_line(clients[games[game_idx].client_indices[1]].socket_fd, "ROLE 1\n");
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG Partie commencée! Vous êtes P1 (pits 0..5). Adversaire: %s\n", 
                             clients[games[game_idx].client_indices[1]].username);
                    send_line(clients[games[game_idx].client_indices[0]].socket_fd, msg);
                    
                    snprintf(msg, sizeof(msg), "MSG Partie commencée! Vous êtes P2 (pits 6..11). Adversaire: %s\n", 
                             clients[games[game_idx].client_indices[0]].username);
                    send_line(clients[games[game_idx].client_indices[1]].socket_fd, msg);
                    
                    broadcast_game_state(&games[game_idx]);
                    
                    printf("[%s] a accepté le défi de [%s] - Partie %d commencée (P1: %s, P2: %s)\n",
                           clients[i].username, challenger,
                           game_idx,
                           clients[games[game_idx].client_indices[0]].username,
                           clients[games[game_idx].client_indices[1]].username);
                }
            }
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
                    
                    clients[i].challenged_by = -1;  
                    send_line(clients[i].socket_fd, "MSG Défi refusé.\n");
                    
                    printf("[%s] a refusé le défi de [%s]\n", clients[i].username, challenger);
                }
            }
            else if (clients[i].status == CLIENT_IN_GAME) {
                Game* g = find_game_for_client(i);
                int game_idx = find_game_index_for_client(i);
                if (g == NULL || game_idx == -1) continue;
                
                int player_id = clients[i].player_id;
                int opponent_idx = clients[i].opponent_index;
                
                if (!strcmp(buf, "QUIT")) {
                    int winner = 1 - player_id;
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "MSG %s a abandonné. Vous gagnez!\n", clients[i].username);
                    send_line(clients[opponent_idx].socket_fd, msg);
                    
                    snprintf(msg, sizeof(msg), "END winner %d\n", winner);
                    send_line(clients[opponent_idx].socket_fd, msg);
                    
                    send_line(clients[i].socket_fd, "MSG Vous avez abandonné.\n");
                    send_line(clients[i].socket_fd, "END forfeit\n");
                    
                    char end_msg[64];
                    snprintf(end_msg, sizeof(end_msg), "END winner %d\n", winner);
                    end_game(g, end_msg, game_idx);
                    
                    printf("%s a abandonné contre %s\n", clients[i].username, clients[opponent_idx].username);
                    continue;
                }
                
                if (g->current_player != player_id) {
                    send_line(clients[i].socket_fd, "MSG Ce n'est pas votre tour.\n");
                    continue;
                }
                
                if (!strncmp(buf, "MOVE ", 5)) {
                    int pit = atoi(buf + 5);
                    
                    printf("[%s] joue le pit %d\n", clients[i].username, pit);
                    
                    memcpy(board, g->board, 12);
                    
                    int last = apply_move_from_pit(player_id, pit);
                    
                    if (last == -2) {
                        send_line(clients[i].socket_fd, "MSG Coup invalide.\n");
                        send_game_state(g, i);  
                        continue;
                    }
                    
                    memcpy(g->board, board, 12);
                    
                    char notify[128];
                    snprintf(notify, sizeof(notify), "MSG %s a déplacé les graines de la case %d.\n", 
                             clients[i].username, pit);
                    send_line(clients[opponent_idx].socket_fd, notify);
                    
                    for (int j = 0; j < g->num_spectators; j++) {
                        int spec_idx = g->spectator_indices[j];
                        if (spec_idx >= 0 && clients[spec_idx].socket_fd > 0) {
                            send_line(clients[spec_idx].socket_fd, notify);
                        }
                    }
                    
                    char gained = collect_seeds((char)player_id, (char)last);
                    g->scores[player_id] += gained;
                    
                    if (g->num_moves < MAX_MOVES) {
                        g->moves[g->num_moves].player = player_id;
                        g->moves[g->num_moves].pit = pit;
                        g->moves[g->num_moves].seeds_captured = gained;
                        g->num_moves++;
                    }
                    
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
                        
                        end_game(g, end_msg, game_idx);
                        continue;
                    }
                    
                    g->current_player = 1 - g->current_player;
                    broadcast_game_state(g);
                }
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
                            
                            end_game(g, "END draw\n", game_idx);
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
    
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd > 0) {
            close(clients[i].socket_fd);
        }
    }
    close(srv);
    
    return 0;
}
