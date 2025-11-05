#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../include/game.h"
#include "../../include/net.h"

#define PORT 4321

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
 * Diffuse l'état actuel du jeu aux deux clients
 * @param c0 Socket du client 0
 * @param c1 Socket du client 1
 */
static void broadcast_state(int c0, int c1) {
    char line[256];
    
    snprintf(line, sizeof(line),
        "STATE %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        board[0], board[1], board[2], board[3], board[4], board[5],
        board[6], board[7], board[8], board[9], board[10], board[11],
        scores[0], scores[1], current_player);
    
    send_line(c0, line);
    send_line(c1, line);
}

int main() {
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
    if (bind(srv, (struct sockaddr*)&a, sizeof(a)) < 0 || listen(srv, 2) < 0) {
        perror("bind/listen");
        return 1;
    }
    
    printf("Server on %d\n", PORT);
    // Acceptation des connexions des deux clients
    int cfd[2];
    for (int i = 0; i < 2; i++) {
        cfd[i] = accept(srv, NULL, NULL);
        if (cfd[i] < 0) {
            perror("accept");
            return 1;
        }
    }
    
    // Attribution des rôles aux clients
    send_line(cfd[0], "ROLE 0\n");
    send_line(cfd[1], "ROLE 1\n");
    
    // Messages d'information aux joueurs
    send_line(cfd[0], "MSG Vous êtes P1 (pits 0..5)\n");
    send_line(cfd[1], "MSG Vous êtes P2 (pits 6..11)\n");
    
    // Initialisation du jeu et envoi de l'état initial
    init_game();
    broadcast_state(cfd[0], cfd[1]);
    // Variables de contrôle du jeu
    int continued = CONTINUE;
    int running = 1;
    
    // Boucle principale du serveur
    while (running) {
        // Préparation du select pour écouter les deux clients
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(cfd[0], &rfds);
        FD_SET(cfd[1], &rfds);
        
        int maxfd = cfd[0] > cfd[1] ? cfd[0] : cfd[1];
        
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) <= 0) {
            continue;
        }
        
        int p = current_player;
        int other = 1 - p;
        // Traitement des messages du joueur actuel
        if (FD_ISSET(cfd[p], &rfds)) {
            char buf[128];
            
            if (recv_line(cfd[p], buf, sizeof(buf)) < 0) {
                running = 0;
                break;
            }
            
            // Traitement d'un coup
            if (!strncmp(buf, "MOVE ", 5)) {
                int pit = atoi(buf + 5);
                int last = apply_move_from_pit(p, pit);
                
                if (last == -2) {
                    send_line(cfd[p], "MSG Coup invalide.\n");
                    continue;
                }
                
                char gained = collect_seeds((char)p, (char)last);
                scores[p] += gained;
                // Vérification de fin de partie
                if (is_game_over((char)continued)) {
                    collect_remaining_seeds((char)continued);
                    broadcast_state(cfd[0], cfd[1]);
                    
                    if (scores[0] == scores[1]) {
                        send_line(cfd[0], "END draw\n");
                        send_line(cfd[1], "END draw\n");
                    } else {
                        int w = (scores[0] > scores[1]) ? 1 : 2;
                        char msg[32];
                        snprintf(msg, sizeof(msg), "END winner %d\n", w);
                        send_line(cfd[0], msg);
                        send_line(cfd[1], msg);
                    }
                    break;
                }
                
                // Changement de joueur et diffusion du nouvel état
                current_player = 1 - current_player;
                broadcast_state(cfd[0], cfd[1]);
            } 
            // Traitement d'une demande d'égalité
            else if (!strcmp(buf, "DRAW")) {
                send_line(cfd[other], "ASKDRAW\n");
                
                char ans[16];
                if (recv_line(cfd[other], ans, sizeof(ans)) < 0) {
                    running = 0;
                    break;
                }
                
                if (!strcmp(ans, "YES")) {
                    continued = DRAW;
                    if (is_game_over((char)continued)) {
                        collect_remaining_seeds((char)continued);
                        broadcast_state(cfd[0], cfd[1]);
                        send_line(cfd[0], "END draw\n");
                        send_line(cfd[1], "END draw\n");
                        break;
                    }
                } else {
                    send_line(cfd[p], "MSG Egalité refusée.\n");
                }
            }
        }
    }
    
    // Nettoyage et fermeture des connexions
    close(cfd[0]);
    close(cfd[1]);
    close(srv);
    
    return 0;
}
