#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>

static int connect_to(const char* ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }
    
    struct sockaddr_in s = {0};
    s.sin_family = AF_INET;
    s.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &s.sin_addr) != 1) {
        perror("inet_pton");
        exit(1);
    }
    
    if (connect(fd, (struct sockaddr*)&s, sizeof(s)) < 0) {
        perror("connect");
        exit(1);
    }
    
    return fd;
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
            return n;
        }
        
        buf[n++] = ch;
    }
    
    buf[n] = 0;
    return n;
}

static void render_state(const int b[12], int s0, int s1, int cur) {
    printf("\n    P2 (%d pts)\n", s1);
    printf("-------------------------\n");
    
    for (int i = 11; i >= 6; i--) {
        printf("%2d  ", i);
    }
    printf("\n");
    
    printf("+---+---+---+---+---+---+\n|");
    for (int i = 11; i >= 6; i--) {
        printf("%2d |", b[i]);
    }
    printf("\n+---+---+---+---+---+---+\n|");
    
    for (int i = 0; i <= 5; i++) {
        printf("%2d |", b[i]);
    }
    printf("\n+---+---+---+---+---+---+\n ");
    
    for (int i = 0; i <= 5; i++) {
        printf("%2d  ", i);
    }
    printf("\n");
    
    printf("--------------------------\n");
    printf("    P1 (%d pts)   (au tour de P%d)\n\n", s0, cur + 1);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    
    int fd = connect_to(argv[1], atoi(argv[2]));
    char buf[256];
    int myrole = -1;
    int myturn = 0;
    int in_game = 0;
    char username[50];
    
    // Demander le username à l'utilisateur
    printf("Entrez votre nom d'utilisateur: ");
    if (!fgets(username, sizeof(username), stdin)) {
        fprintf(stderr, "Erreur de lecture du username\n");
        close(fd);
        return 1;
    }
    
    // Supprimer le '\n' à la fin
    size_t len = strlen(username);
    if (len > 0 && username[len - 1] == '\n') {
        username[len - 1] = '\0';
    }
    
    printf("Connexion au serveur...\n");
    printf("\nCommandes disponibles:\n");
    printf("  list              - Voir les joueurs disponibles\n");
    printf("  challenge <nom>   - Défier un joueur\n");
    printf("  accept <nom>      - Accepter un défi\n");
    printf("  refuse <nom>      - Refuser un défi\n\n");
    
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        
        int maxfd = fd > STDIN_FILENO ? fd : STDIN_FILENO;
        
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) <= 0) {
            continue;
        }
        
        // Message du serveur
        if (FD_ISSET(fd, &rfds)) {
            if (recv_line(fd, buf, sizeof(buf)) < 0) {
                puts("Déconnecté.");
                break;
            }
            
            // Gérer la demande d'enregistrement
            if (!strncmp(buf, "REGISTER", 8)) {
                char msg[128];
                snprintf(msg, sizeof(msg), "USERNAME %s\n", username);
                send(fd, msg, strlen(msg), 0);
            }
            // Liste des utilisateurs
            else if (!strncmp(buf, "USERLIST", 8)) {
                printf("\n=== Joueurs disponibles ===\n");
                if (strlen(buf) > 9) {
                    char* token = strtok(buf + 9, " ");
                    while (token != NULL) {
                        printf("  - %s\n", token);
                        token = strtok(NULL, " ");
                    }
                } else {
                    printf("  Aucun joueur disponible.\n");
                }
                printf("===========================\n\n");
                
                if (!in_game) {
                    printf("> ");
                    fflush(stdout);
                }
            }
            // Défi reçu
            else if (!strncmp(buf, "CHALLENGED_BY ", 14)) {
                char challenger[50];
                strncpy(challenger, buf + 14, sizeof(challenger) - 1);
                challenger[sizeof(challenger) - 1] = '\0';
                
                printf("\n*** %s vous défie! ***\n", challenger);
                printf("Tapez 'accept %s' pour accepter ou 'refuse %s' pour refuser\n\n", challenger, challenger);
                
                if (!in_game) {
                    printf("> ");
                    fflush(stdout);
                }
            }
            else if (!strncmp(buf, "ROLE ", 5)) {
                myrole = atoi(buf + 5);
                in_game = 1;
                printf("Vous êtes P%d.\n", myrole + 1);
            }
            else if (!strncmp(buf, "STATE ", 6)) {
                int b[12], s0, s1, cur;
                
                if (sscanf(buf + 6, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
                    &b[6], &b[7], &b[8], &b[9], &b[10], &b[11],
                    &s0, &s1, &cur) == 15) {
                    
                    render_state(b, s0, s1, cur);
                    myturn = (myrole == cur);
                    
                    if (myturn) {
                        printf("Votre tour. Entrez un pit (ou 'd' pour DRAW): ");
                        fflush(stdout);
                    }
                }
            }
            else if (!strncmp(buf, "ASKDRAW", 7)) {
                printf("L'adversaire propose l'égalité. Accepter ? (o/n): ");
                fflush(stdout);
                
                char response[16];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] == 'o' || response[0] == 'O') {
                        send(fd, "YES\n", 4, 0);
                    } else {
                        send(fd, "NO\n", 3, 0);
                    }
                }
            }
            else if (!strncmp(buf, "MSG ", 4)) {
                puts(buf + 4);
                
                if (myturn && strstr(buf + 4, "invalide")) {
                    printf("Votre tour. Entrez un pit (ou 'd' pour DRAW): ");
                    fflush(stdout);
                } else if (!in_game && !strstr(buf + 4, "Bienvenue")) {
                    printf("> ");
                    fflush(stdout);
                }
            }
            else if (!strncmp(buf, "END ", 4)) {
                puts(buf);
                in_game = 0;
                myturn = 0;
                printf("\nPartie terminée. Vous pouvez défier un autre joueur.\n");
                printf("> ");
                fflush(stdout);
            }
            else {
                puts(buf);
            }
        }
        
        // Entrée utilisateur
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            if (!fgets(buf, sizeof(buf), stdin)) {
                break;
            }
            
            // Supprimer le '\n'
            len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') {
                buf[len - 1] = '\0';
            }
            
            if (in_game && myturn) {
                // En partie, commandes de jeu
                if (buf[0] == 'd' || buf[0] == 'D') {
                    send(fd, "DRAW\n", 5, 0);
                } else {
                    char out[64];
                    snprintf(out, sizeof(out), "MOVE %s\n", buf);
                    send(fd, out, strlen(out), 0);
                }
                myturn = 0;
            } else if (!in_game) {
                // Hors partie, commandes de lobby
                if (!strcmp(buf, "list")) {
                    send(fd, "LIST\n", 5, 0);
                } else if (!strncmp(buf, "challenge ", 10)) {
                    char out[128];
                    snprintf(out, sizeof(out), "CHALLENGE %s\n", buf + 10);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(buf, "accept ", 7)) {
                    char out[128];
                    snprintf(out, sizeof(out), "ACCEPT %s\n", buf + 7);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(buf, "refuse ", 7)) {
                    char out[128];
                    snprintf(out, sizeof(out), "REFUSE %s\n", buf + 7);
                    send(fd, out, strlen(out), 0);
                } else {
                    printf("Commande inconnue. Tapez 'list' pour voir les joueurs.\n");
                    printf("> ");
                    fflush(stdout);
                }
            }
        }
    }
    
    close(fd);
    return 0;
}
