/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 21/10/2025
    copyright            : (C) 2025 par Mohamed et Diego
    e-mail               : mohamed.lemseffer@insa-lyon.fr / diego.aquinoh@insa-lyon.fr / 

*************************************************************************/

#include "../../include/game.h"

#include <stdio.h>
#include <stdlib.h>

char board[12];
char scores[2];
char current_player;
char game_over = 0;

enum {PROPOSITION_DRAW = -1};
enum {DRAW = 0, CONTINUE = 1};

#ifdef GAME_STANDALONE
int main(void)
{
    game_loop();
    return 0;
}
#endif

/**
 * Initialise le plateau de jeu
 */
void init_game()
{
    for (int i = 0; i < 12; i++)
    {
        board[i] = 4;
    }
    scores[0] = 0;
    scores[1] = 0;
    current_player = 0;
    game_over = 0;
    display_board();
}

/**
 * Affiche le plateau de jeu
 */
void display_board()
{
    printf("\n");
    printf("    P2 (%d pts)\n", scores[1]);
    printf("-------------------------\n");
    for (int i = 11; i >= 6; i--)
    {
        printf("%2d  ", i);
    }
    printf("\n");
    printf("+---+---+---+---+---+---+\n");
    printf("|");
    for (int i = 11; i >= 6; i--)
    {
        printf("%2d |", board[i]);
    }
    printf("\n");
    printf("+---+---+---+---+---+---+\n");
    printf("|");
    for (int i = 0; i <= 5; i++)
    {
        printf("%2d |", board[i]);
    }
    printf("\n");
    printf("+---+---+---+---+---+---+\n");
    printf(" ");
    for (int i = 0; i <= 5; i++)
    {
        printf("%2d  ", i);
    }
    printf("\n");

    printf("--------------------------\n");
    printf("    P1 (%d pts)\n", scores[0]);
    printf("\n");
}

/**
 * Joue un tour pour un joueur
 */
char play_turn(char player)
{
    char last_pit_index = move(player);
    if (last_pit_index == PROPOSITION_DRAW)
    {
        return DRAW;
    }
    char new_score = collect_seeds(player, last_pit_index);
    scores[player] += new_score;
    display_board();

    return CONTINUE;
}

/**
 * Collecte les graines selon les règles de capture
 */
char collect_seeds(char player, char last_pit_index)
{
    if (((player == 0) && (last_pit_index >= 6 && last_pit_index <= 11) ||
         (player == 1) && (last_pit_index >= 0 && last_pit_index < 6))

        && (board[last_pit_index] == 2 || board[last_pit_index] == 3))
    {
        char collected_seeds = board[last_pit_index];
        board[last_pit_index] = 0;
        return collected_seeds + collect_seeds(player, (last_pit_index - 1 + 12) % 12);
    }
    return 0;
}

/**
 * Effectue un mouvement pour un joueur
 */
char move(char player)
{
    char is_valid = 0;
    int pit_index = -1;
    while (!is_valid)
    {
        printf("Player %d's turn. Choose a pit (0-5 for P1, 6-11 for P2): ", player + 1);
        printf(" (or enter 255 to propose a draw) ");
        scanf("%d", &pit_index);
        if (pit_index == 255)
        {
            char response = 0;
            response = proposition_draw(player);
            if (response)
            {
                game_over = 1;
                return PROPOSITION_DRAW;
            }
            continue;
        }
        if ((player == 0 && (pit_index < 0 || pit_index > 5)) ||
            (player == 1 && (pit_index < 6 || pit_index > 11)) ||
            board[pit_index] == 0)
        {
            printf("Invalid move. Try again.\n");
            is_valid = 0;
        }
        else
        {
            is_valid = 1;
        }
    }

    int seeds = board[pit_index];
    board[pit_index] = 0;
    int index = pit_index;

    while (seeds > 0)
    {
        index = (index + 1) % 12;
        if (index != pit_index)
        {
            board[index]++;
            seeds--;
        }
    }
    return index;
}

/**
 * Change le joueur actuel
 */
void switch_player()
{
    current_player = 1 - current_player;
}

/**
 * Vérifie si la partie est terminée
 */
char is_game_over(char continued)
{
    if (continued == DRAW)
    {
        return 1;
    }
    
    int side1_empty = 1;
    int side2_empty = 1;
    
    for (int i = 0; i < 6; i++)
    {
        if (board[i] > 0)
        {
            side1_empty = 0;
            break;
        }
    }

    for (int i = 6; i < 12; i++)
    {
        if (board[i] > 0)
        {
            side2_empty = 0;
            break;
        }
    }

    if (side1_empty || side2_empty || scores[0] >= 25 || scores[1] >= 25)
    {
        game_over = 1;
    }

    return game_over;
}

/**
 * Collecte les graines restantes à la fin de la partie
 */
void collect_remaining_seeds(char continued)
{
    if (continued == DRAW)
    {
        return;
    }
    for (int i = 0; i < 12; i++)
    {
        scores[i < 6 ? 0 : 1] += board[i];
        board[i] = 0;
    }
}

/**
 * Affiche le résultat final de la partie
 */
void declare_winner(char continued)
{
    printf("Final Scores:\n");
    printf("Player 1: %d\n", scores[0]);
    printf("Player 2: %d\n", scores[1]);

    if (continued == DRAW)
    {
        printf("The game ended in a draw by agreement.\n");
        return;
    }
    if (scores[0] > scores[1])
    {
        printf("Player 1 wins!\n");
    }
    else if (scores[1] > scores[0])
    {
        printf("Player 2 wins!\n");
    }
    else
    {
        printf("It's a tie!\n");
    }
}

/**
 * Propose un match nul à l'adversaire
 */
char proposition_draw(char player)
{
    char response;
    printf("Player %d proposes a draw to Player %d. Do you accept? (1 for Yes, 0 for No): ", player + 1, 2 - player);
    scanf(" %c", &response);
    return response == '1';
}

/**
 * Boucle principale du jeu
 */
void game_loop()
{
    init_game();
    int continued = CONTINUE;
    while (!is_game_over(continued))
    {
        continued = play_turn(current_player);
        switch_player();
    }
    collect_remaining_seeds(continued);
    declare_winner(continued);
}
