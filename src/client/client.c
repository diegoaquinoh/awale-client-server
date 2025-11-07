#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>

static void print_help(void) {
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║         COMMANDES DISPONIBLES          ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ /list              - Joueurs en ligne  ║\n");
    printf("║ /games             - Parties en cours  ║\n");
    printf("║ /watch <id>        - Regarder partie   ║\n");
    printf("║ /stopwatch         - Arrêter regarder  ║\n");
    printf("║ /challenge <nom>   - Défier joueur     ║\n");
    printf("║ /accept <nom>      - Accepter défi     ║\n");
    printf("║ /refuse <nom>      - Refuser défi      ║\n");
    printf("║ /board             - Afficher plateau  ║\n");
    printf("║ /bio               - Définir votre bio ║\n");
    printf("║ /whois <nom>       - Voir bio joueur   ║\n");
    printf("║ /help              - Cette aide        ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ En partie:                             ║\n");
    printf("║ /0 à /11           - Jouer une case    ║\n");
    printf("║ /d                 - Proposer égalité  ║\n");
    printf("║ /q                 - Abandonner        ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ @<nom> <msg>       - Message privé     ║\n");
    printf("║ <message>          - Message public    ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
}

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
    int editing_bio = 0;  // Flag pour savoir si on est en mode édition de bio
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
    print_help();
    
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
            // Liste des parties
            else if (!strncmp(buf, "GAMESLIST", 9)) {
                printf("\n=== Parties en cours ===\n");
                if (strlen(buf) > 10) {
                    char* token = strtok(buf + 10, " ");
                    while (token != NULL) {
                        // Format: id:player1_vs_player2
                        char* colon = strchr(token, ':');
                        if (colon) {
                            *colon = '\0';
                            char* game_info = colon + 1;
                            // Remplacer les _ par des espaces pour l'affichage
                            for (char* p = game_info; *p; p++) {
                                if (*p == '_') *p = ' ';
                            }
                            printf("  [%s] %s\n", token, game_info);
                        }
                        token = strtok(NULL, " ");
                    }
                } else {
                    printf("  Aucune partie en cours.\n");
                }
                printf("========================\n\n");
                
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
                printf("Tapez '/accept %s' pour accepter ou '/refuse %s' pour refuser\n\n", challenger, challenger);
                
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
                        printf("Votre tour (/0-11, /d, /q ou message): ");
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
                // Détecter le début/continuation de l'édition de bio (Ligne X:)
                if (strstr(buf + 4, "Ligne ") && strstr(buf + 4, ":")) {
                    editing_bio = 1;
                    // Afficher sans le saut de ligne final pour que l'utilisateur tape sur la même ligne
                    char msg_copy[256];
                    strncpy(msg_copy, buf + 4, sizeof(msg_copy) - 1);
                    msg_copy[sizeof(msg_copy) - 1] = '\0';
                    // Enlever le \n à la fin s'il existe
                    size_t len = strlen(msg_copy);
                    if (len > 0 && msg_copy[len - 1] == '\n') {
                        msg_copy[len - 1] = '\0';
                    }
                    printf("%s", msg_copy);
                    fflush(stdout);
                }
                // Détecter la fin de l'édition de bio
                else if (strstr(buf + 4, "Bio enregistrée")) {
                    editing_bio = 0;
                    puts(buf + 4);
                    printf("> ");
                    fflush(stdout);
                }
                else {
                    puts(buf + 4);
                    if (myturn && strstr(buf + 4, "invalide")) {
                        printf("Votre tour (/0-11, /d, /q ou message): ");
                        fflush(stdout);
                    } else if (!in_game && !strstr(buf + 4, "Bienvenue") && !editing_bio) {
                        printf("> ");
                        fflush(stdout);
                    }
                }
            }
            // Message de chat
            else if (!strncmp(buf, "CHAT ", 5)) {
                printf("\n💬 %s\n", buf + 5);
                // Ne pas réafficher le prompt automatiquement
            }
            // Bio reçue
            else if (!strncmp(buf, "BIO", 3)) {
                // Afficher la bio complète (multi-lignes)
                printf("%s\n", buf + 4);  // Sauter "BIO\n"
                
                // Lire les lignes suivantes jusqu'à trouver "=========="
                while (1) {
                    if (recv_line(fd, buf, sizeof(buf)) < 0) {
                        break;
                    }
                    printf("%s\n", buf);
                    if (strstr(buf, "========")) {
                        break;
                    }
                }
                
                if (!in_game) {
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
            
            // Ligne vide
            if (strlen(buf) == 0) {
                // En mode édition de bio, envoyer la ligne vide pour terminer
                if (editing_bio) {
                    send(fd, "\n", 1, 0);
                }
                // Sinon, ignorer
                continue;
            }
            
            // Commandes commençant par '/'
            if (buf[0] == '/') {
                // Extraire la commande
                char* cmd = buf + 1;
                
                if (!strcmp(cmd, "help")) {
                    print_help();
                    if (!in_game) {
                        printf("> ");
                        fflush(stdout);
                    }
                } else if (!strcmp(cmd, "q")) {
                    send(fd, "QUIT\n", 5, 0);
                    if (in_game && myturn) myturn = 0;
                } else if (!strcmp(cmd, "d")) {
                    send(fd, "DRAW\n", 5, 0);
                    if (in_game && myturn) myturn = 0;
                } else if (!strcmp(cmd, "list")) {
                    send(fd, "LIST\n", 5, 0);
                } else if (!strcmp(cmd, "games")) {
                    send(fd, "GAMES\n", 6, 0);
                } else if (!strcmp(cmd, "stopwatch")) {
                    send(fd, "STOPWATCH\n", 10, 0);
                } else if (!strcmp(cmd, "board")) {
                    send(fd, "BOARD\n", 6, 0);
                } else if (!strncmp(cmd, "watch ", 6)) {
                    char out[128];
                    snprintf(out, sizeof(out), "WATCH %s\n", cmd + 6);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(cmd, "challenge ", 10)) {
                    char out[128];
                    snprintf(out, sizeof(out), "CHALLENGE %s\n", cmd + 10);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(cmd, "accept ", 7)) {
                    char out[128];
                    snprintf(out, sizeof(out), "ACCEPT %s\n", cmd + 7);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(cmd, "refuse ", 7)) {
                    char out[128];
                    snprintf(out, sizeof(out), "REFUSE %s\n", cmd + 7);
                    send(fd, out, strlen(out), 0);
                } else if (!strcmp(cmd, "bio")) {
                    // Envoyer simplement la commande BIO, le serveur gérera l'édition ligne par ligne
                    send(fd, "BIO\n", 4, 0);
                } else if (!strncmp(cmd, "whois ", 6)) {
                    char out[128];
                    snprintf(out, sizeof(out), "WHOIS %s\n", cmd + 6);
                    send(fd, out, strlen(out), 0);
                } else {
                    // Vérifier si c'est un chiffre pour jouer (0-11)
                    char* endptr;
                    long pit = strtol(cmd, &endptr, 10);
                    
                    if (*endptr == '\0' && pit >= 0 && pit <= 11) {
                        // C'est un numéro de pit valide
                        if (in_game && myturn) {
                            char out[300];
                            snprintf(out, sizeof(out), "MOVE %ld\n", pit);
                            send(fd, out, strlen(out), 0);
                            myturn = 0;
                        } else if (in_game && !myturn) {
                            printf("Ce n'est pas votre tour.\n");
                        } else {
                            printf("Vous n'êtes pas en partie.\n");
                        }
                    } else {
                        printf("Commande inconnue. Tapez '/help' pour voir les commandes.\n");
                        if (!in_game) {
                            printf("> ");
                            fflush(stdout);
                        }
                    }
                }
            } else {
                // Pas de '/', vérifier si on est en mode édition de bio
                if (editing_bio) {
                    // En mode édition de bio, envoyer le texte brut
                    char out[300];
                    snprintf(out, sizeof(out), "%s\n", buf);
                    send(fd, out, strlen(out), 0);
                }
                // Sinon, vérifier si c'est un message privé avec @username
                else if (buf[0] == '@') {
                    // Message privé : @username message
                    char* space = strchr(buf + 1, ' ');
                    if (space) {
                        *space = '\0';
                        char out[512];
                        snprintf(out, sizeof(out), "CHAT @%s %s\n", buf + 1, space + 1);
                        send(fd, out, strlen(out), 0);
                    } else {
                        printf("Usage: @<username> <message>\n");
                    }
                } else {
                    // Message de chat normal
                    char out[512];
                    snprintf(out, sizeof(out), "CHAT %s\n", buf);
                    send(fd, out, strlen(out), 0);
                }
                
                // Ne pas réafficher le prompt après un message de chat
            }
        }
    }
    
    close(fd);
    return 0;
}
