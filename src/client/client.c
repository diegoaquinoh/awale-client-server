/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 21/10/2025
    copyright            : (C) 2025 par Mohamed et Diego
    e-mail               : mohamed.lemseffer@insa-lyon.fr / diego.aquinoh@insa-lyon.fr / 

*************************************************************************/


#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>

// Codes de couleur ANSI (versions sombres)
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BROWN   "\033[38;5;94m"
#define COLOR_BOLD    "\033[1m"

// Buffer global pour la saisie utilisateur
static char input_buffer[256] = "";
static int input_pos = 0;

static void clear_current_line(void) {
    printf("\r\033[K");
    fflush(stdout);
}

static void redisplay_prompt(const char* prompt) {
    printf("%s%s", prompt, input_buffer);
    fflush(stdout);
}

static void show_prompt(void) {
    printf(COLOR_BLUE "> " COLOR_RESET);
    redisplay_prompt("");
}

static void print_help(void) {
    printf("\n" COLOR_MAGENTA "╔═══════════════════════════════════════════╗\n");
    printf("║          COMMANDES DISPONIBLES            ║\n");
    printf("╠═══════════════════════════════════════════╣\n" COLOR_RESET);
    printf(COLOR_MAGENTA "║" COLOR_RESET " " COLOR_BLUE "/list" COLOR_RESET "                - Joueurs en ligne   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/games" COLOR_RESET "               - Parties en cours   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/watch <id>" COLOR_RESET "          - Regarder partie    " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/stopwatch" COLOR_RESET "           - Arrêter regarder   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/challenge <nom>" COLOR_RESET "     - Défier joueur      " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/accept <nom>" COLOR_RESET "        - Accepter défi      " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/refuse <nom>" COLOR_RESET "        - Refuser défi       " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/board" COLOR_RESET "               - Afficher plateau   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/bio" COLOR_RESET "                 - Définir votre bio  " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/whois <nom>" COLOR_RESET "         - Voir bio joueur    " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/history" COLOR_RESET "             - Parties jouées     " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/replay <num>" COLOR_RESET "        - Revoir une partie  " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/help" COLOR_RESET "                - Cette aide         " COLOR_MAGENTA "║\n");
    printf(COLOR_MAGENTA "╠═══════════════════════════════════════════╣\n");
    printf("║" COLOR_RESET " " COLOR_YELLOW "Gestion des amis:" COLOR_RESET "                         " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/addfriend <nom>" COLOR_RESET "     - Demande d'ami      " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/acceptfriend <nom>" COLOR_RESET "  - Accepter demande   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/friendrequests" COLOR_RESET "      - Demandes reçues    " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/removefriend <nom>" COLOR_RESET "  - Retirer un ami     " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/friends" COLOR_RESET "             - Liste de vos amis  " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/private" COLOR_RESET "             - Toggle mode privé  " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/save" COLOR_RESET "                - Toggle auto-save   " COLOR_MAGENTA "║\n");
    printf(COLOR_MAGENTA "╠═══════════════════════════════════════════╣\n");
    printf("║" COLOR_RESET " " COLOR_YELLOW "En partie:" COLOR_RESET "                                " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/0 à /11" COLOR_RESET "             - Jouer une case     " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/d" COLOR_RESET "                   - Proposer égalité   " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "/q" COLOR_RESET "                   - Abandonner         " COLOR_MAGENTA "║\n");
    printf(COLOR_MAGENTA "╠═══════════════════════════════════════════╣\n");
    printf("║" COLOR_RESET " " COLOR_BLUE "@<nom> <msg>" COLOR_RESET "         - Message privé      " COLOR_MAGENTA "║\n");
    printf("║" COLOR_RESET " <message>            - Message public     " COLOR_MAGENTA "║\n");
    printf("╚═══════════════════════════════════════════╝" COLOR_RESET "\n\n");
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
    printf("\n" COLOR_BLUE "    P2 (%d pts)" COLOR_RESET "\n", s1);
    printf(COLOR_MAGENTA "-------------------------" COLOR_RESET "\n");
    
    printf(COLOR_YELLOW " ");
    for (int i = 11; i >= 6; i--) {
        printf("%2d  ", i);
    }
    printf(COLOR_RESET "\n");
    
    printf(COLOR_MAGENTA "+---+---+---+---+---+---+\n" COLOR_RESET);
    printf(COLOR_MAGENTA "|" COLOR_RESET);
    for (int i = 11; i >= 6; i--) {
        printf(COLOR_BROWN "%2d " COLOR_RESET COLOR_MAGENTA "|" COLOR_RESET, b[i]);
    }
    printf("\n" COLOR_MAGENTA "+---+---+---+---+---+---+\n" COLOR_RESET);
    printf(COLOR_MAGENTA "|" COLOR_RESET);
    for (int i = 0; i <= 5; i++) {
        printf(COLOR_BROWN "%2d " COLOR_RESET COLOR_MAGENTA "|" COLOR_RESET, b[i]);
    }
    printf("\n" COLOR_MAGENTA "+---+---+---+---+---+---+" COLOR_RESET "\n");
    
    printf(COLOR_YELLOW " ");
    for (int i = 0; i <= 5; i++) {
        printf("%2d  ", i);
    }
    printf(COLOR_RESET "\n");
    
    printf(COLOR_MAGENTA "--------------------------" COLOR_RESET "\n");
    printf(COLOR_GREEN "    P1 (%d pts)" COLOR_RESET "   (au tour de " COLOR_BOLD COLOR_BLUE "P%d" COLOR_RESET ")\n\n", s0, cur + 1);
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
    printf(COLOR_BLUE "👤 Entrez votre nom d'utilisateur: " COLOR_RESET);
    if (!fgets(username, sizeof(username), stdin)) {
        fprintf(stderr, COLOR_RED "✗ Erreur de lecture du username\n" COLOR_RESET);
        close(fd);
        return 1;
    }
    
    // Supprimer le '\n' à la fin
    size_t len = strlen(username);
    if (len > 0 && username[len - 1] == '\n') {
        username[len - 1] = '\0';
    }
    
    printf(COLOR_GREEN "✓ Connexion au serveur...\n" COLOR_RESET);
    print_help();
    show_prompt();  // Afficher le prompt initial
    
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
            // Effacer la ligne courante si on est en train de taper
            if (input_pos > 0) {
                clear_current_line();
            }
            
            if (recv_line(fd, buf, sizeof(buf)) < 0) {
                printf(COLOR_RED "✗ Déconnecté du serveur.\n" COLOR_RESET);
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
                printf("\n" COLOR_MAGENTA "=== Joueurs disponibles ===" COLOR_RESET "\n");
                if (strlen(buf) > 9) {
                    char* token = strtok(buf + 9, " ");
                    while (token != NULL) {
                        printf("  " COLOR_GREEN "• " COLOR_RESET "%s\n", token);
                        token = strtok(NULL, " ");
                    }
                } else {
                    printf("  " COLOR_YELLOW "Aucun joueur disponible.\n" COLOR_RESET);
                }
                printf(COLOR_MAGENTA "===========================" COLOR_RESET "\n\n");
                
                if (!in_game) {
                    printf(COLOR_BLUE "> " COLOR_RESET);
                    redisplay_prompt("");
                }
            }
            // Liste des parties
            else if (!strncmp(buf, "GAMESLIST", 9)) {
                printf("\n" COLOR_MAGENTA "=== Parties en cours ===" COLOR_RESET "\n");
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
                            printf("  " COLOR_YELLOW "[%s]" COLOR_RESET " %s\n", token, game_info);
                        }
                        token = strtok(NULL, " ");
                    }
                } else {
                    printf("  " COLOR_YELLOW "Aucune partie en cours.\n" COLOR_RESET);
                }
                printf(COLOR_MAGENTA "========================" COLOR_RESET "\n\n");
                
                if (!in_game) {
                    show_prompt();
                }
            }
            // Défi reçu
            else if (!strncmp(buf, "CHALLENGED_BY ", 14)) {
                char challenger[50];
                strncpy(challenger, buf + 14, sizeof(challenger) - 1);
                challenger[sizeof(challenger) - 1] = '\0';
                
                printf("\n" COLOR_YELLOW "⚔️  %s vous défie! ⚔️" COLOR_RESET "\n", challenger);
                printf("Tapez " COLOR_GREEN "'/accept %s'" COLOR_RESET " ou " COLOR_RED "'/refuse %s'" COLOR_RESET " pour répondre\n\n", challenger, challenger);
                
                if (!in_game) {
                    show_prompt();
                }
            }
            else if (!strncmp(buf, "ROLE ", 5)) {
                myrole = atoi(buf + 5);
                in_game = 1;
                printf(COLOR_BLUE "✓ Vous êtes P%d\n" COLOR_RESET, myrole + 1);
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
                        printf(COLOR_GREEN "➤ À vous de jouer" COLOR_RESET " (/0-11, /d, /q): ");
                        fflush(stdout);
                    }
                }
            }
            else if (!strncmp(buf, "ASKDRAW", 7)) {
                printf(COLOR_YELLOW "⚠️  L'adversaire propose l'égalité. Accepter ? (o/n): " COLOR_RESET);
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
            else if (!strncmp(buf, "ASKSAVE", 7)) {
                printf(COLOR_BLUE "💾 Sauvegarder cette partie ? (o/n): " COLOR_RESET);
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
                    printf(COLOR_GREEN "✓ %s\n" COLOR_RESET, buf + 4);
                    show_prompt();
                }
                else {
                    // Colorer les messages d'erreur en rouge
                    if (strstr(buf + 4, "invalide") || strstr(buf + 4, "Erreur") || 
                        strstr(buf + 4, "impossible") || strstr(buf + 4, "n'existe pas")) {
                        printf(COLOR_RED "✗ %s\n" COLOR_RESET, buf + 4);
                    }
                    // Messages de succès en vert
                    else if (strstr(buf + 4, "accepté") || strstr(buf + 4, "ajouté") || 
                             strstr(buf + 4, "activé") || strstr(buf + 4, "Sauvegarde")) {
                        printf(COLOR_GREEN "✓ %s\n" COLOR_RESET, buf + 4);
                    }
                    // Messages info en cyan
                    else if (strstr(buf + 4, "Mode privé") || strstr(buf + 4, "Mode auto-save")) {
                        printf(COLOR_BLUE "ℹ %s\n" COLOR_RESET, buf + 4);
                    }
                    else {
                        puts(buf + 4);
                    }
                    
                    if (myturn && strstr(buf + 4, "invalide")) {
                        printf(COLOR_GREEN "➤ À vous de jouer" COLOR_RESET " (/0-11, /d, /q): ");
                        redisplay_prompt("");
                    } else if (!in_game && !editing_bio) {
                        show_prompt();
                    }
                }
            }
            // Affichage d'une partie rejouée
            else if (!strncmp(buf, "REPLAY", 6)) {
                // Afficher tout le contenu de la partie (tout est déjà dans buf après "REPLAY\n")
                printf("\n" COLOR_BLUE "%s\n" COLOR_RESET, buf + 7);  // Afficher après "REPLAY\n"
                if (!in_game) {
                    show_prompt();
                }
            }
            // Message de chat
            else if (!strncmp(buf, "CHAT ", 5)) {
                printf("\n" COLOR_YELLOW "💬 %s\n" COLOR_RESET, buf + 5);
                // Réafficher le prompt avec le texte en cours
                if (!in_game) {
                    show_prompt();
                } else if (myturn) {
                    printf(COLOR_GREEN "➤ À vous de jouer" COLOR_RESET " (/0-11, /d, /q): ");
                    redisplay_prompt("");
                }
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
                    show_prompt();
                }
            }
            else if (!strncmp(buf, "END ", 4)) {
                printf(COLOR_YELLOW "🏁 %s\n" COLOR_RESET, buf);
                in_game = 0;
                myturn = 0;
                printf("\n" COLOR_GREEN "✓ Partie terminée!\n" COLOR_RESET);
                show_prompt();
            }
            else {
                puts(buf);
            }
        }
        
        // Entrée utilisateur
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) <= 0) {
                break;
            }
            
            // Enter pressé - traiter la ligne
            if (c == '\n') {
                // Copier le buffer dans buf pour traitement
                strncpy(buf, input_buffer, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                
                // Réinitialiser le buffer de saisie
                input_buffer[0] = '\0';
                input_pos = 0;
                
                // Effacer la ligne actuelle (contient le texte tapé)
                clear_current_line();
                
                // Ligne vide
                if (strlen(buf) == 0) {
                    // En mode édition de bio, envoyer la ligne vide pour terminer
                    if (editing_bio) {
                        send(fd, "\n", 1, 0);
                    }
                    // Sinon, ignorer et réafficher le prompt
                    else if (!in_game) {
                        show_prompt();
                    } else if (myturn) {
                        printf(COLOR_GREEN "➤ À vous de jouer" COLOR_RESET " (/0-11, /d, /q): ");
                        fflush(stdout);
                    }
                    continue;
                }
            }
            // Backspace/Delete
            else if (c == 127 || c == 8) {
                if (input_pos > 0) {
                    input_pos--;
                    input_buffer[input_pos] = '\0';
                    printf("\b \b");  // Effacer le caractère
                    fflush(stdout);
                }
                continue;
            }
            // Caractère normal
            else if (c >= 32 && c < 127 && input_pos < sizeof(input_buffer) - 1) {
                input_buffer[input_pos++] = c;
                input_buffer[input_pos] = '\0';
                printf("%c", c);  // Afficher le caractère
                fflush(stdout);
                continue;
            }
            // Caractère de contrôle ignoré
            else {
                continue;
            }
            
            // Commandes commençant par '/'
            if (buf[0] == '/') {
                // Extraire la commande
                char* cmd = buf + 1;
                
                if (!strcmp(cmd, "help")) {
                    print_help();
                    if (!in_game) {
                        show_prompt();
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
                } else if (!strncmp(cmd, "addfriend ", 10)) {
                    char out[128];
                    snprintf(out, sizeof(out), "ADDFRIEND %s\n", cmd + 10);
                    send(fd, out, strlen(out), 0);
                } else if (!strncmp(cmd, "acceptfriend ", 13)) {
                    char out[128];
                    snprintf(out, sizeof(out), "ACCEPTFRIEND %s\n", cmd + 13);
                    send(fd, out, strlen(out), 0);
                } else if (!strcmp(cmd, "friendrequests") || !strcmp(cmd, "listfriendrequests")) {
                    send(fd, "LISTFRIENDREQUESTS\n", 19, 0);
                } else if (!strncmp(cmd, "removefriend ", 13)) {
                    char out[128];
                    snprintf(out, sizeof(out), "REMOVEFRIEND %s\n", cmd + 13);
                    send(fd, out, strlen(out), 0);
                } else if (!strcmp(cmd, "friends") || !strcmp(cmd, "listfriends")) {
                    send(fd, "LISTFRIENDS\n", 12, 0);
                } else if (!strcmp(cmd, "private")) {
                    send(fd, "PRIVATE\n", 8, 0);
                } else if (!strcmp(cmd, "save")) {
                    send(fd, "SAVE\n", 5, 0);
                } else if (!strcmp(cmd, "history")) {
                    send(fd, "HISTORY\n", 8, 0);
                } else if (!strncmp(cmd, "replay ", 7)) {
                    char out[128];
                    snprintf(out, sizeof(out), "REPLAY %s\n", cmd + 7);
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
                            printf(COLOR_RED "✗ Ce n'est pas votre tour.\n" COLOR_RESET);
                        } else {
                            printf(COLOR_RED "✗ Vous n'êtes pas en partie.\n" COLOR_RESET);
                        }
                    } else {
                        printf(COLOR_RED "✗ Commande inconnue. Tapez '/help' pour voir les commandes.\n" COLOR_RESET);
                        if (!in_game) {
                            show_prompt();
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
                        printf(COLOR_RED "✗ Usage: @<username> <message>\n" COLOR_RESET);
                        if (!in_game) {
                            show_prompt();
                        }
                    }
                } else {
                    // Message de chat normal
                    char out[512];
                    snprintf(out, sizeof(out), "CHAT %s\n", buf);
                    send(fd, out, strlen(out), 0);
                }
                
                // Réafficher le prompt après un message de chat
                if (!in_game) {
                    show_prompt();
                } else if (myturn) {
                    printf(COLOR_GREEN "➤ À vous de jouer" COLOR_RESET " (/0-11, /d, /q): ");
                    fflush(stdout);
                }
            }
        }
    }
    
    close(fd);
    return 0;
}
