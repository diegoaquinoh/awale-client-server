#include "../../include/game.h"
#include <stdio.h>
#include <stdlib.h>

int main()
{
    game_loop();
    return 0;
}

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
}

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

void play_turn(char player)
{

    char last_pit_index = move(player);
    char new_score = collect_seeds(player, last_pit_index);
    scores[player] += new_score;

    return;
}

char collect_seeds(char player, char last_pit_index) //Ne pas re remplir le trou d'origine
{
    if (((player == 1)&&(last_pit_index<6 && last_pit_index>=0) ||
         (player == 0)&&(last_pit_index>5 && last_pit_index<=11))
    
    &&(board[last_pit_index] == 2 || board[last_pit_index] == 3))
    {
        char collected = board[last_pit_index];
        board[last_pit_index] = 0;
        return collected + collect_seeds(player, (last_pit_index - 1 + 12) % 12);
    }
    return collected;
}

char move(char player)
{
    char is_valid = 0;
    int pit_index = -1;
    while (!is_valid)
    {
        printf("Player %d's turn. Choose a pit (0-5 for P1, 6-11 for P2): ", player + 1);
        scanf("%d", &pit_index);
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
        board[index]++;
        seeds--;
    }
    return index;

    // Capture logic can be added here
}

void switch_player()
{
    current_player = 1 - current_player;
}

char is_game_over()
{

    // TODO: implement more conditions for game over
    int side1_empty = 1;
    int side2_empty = 1;
    //3 cases: side 1 empty, side 2 empty, 
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

void collect_remaining_seeds()
{
    for (int i = 0; i < 12; i++)
    {
        scores[i < 6 ? 0 : 1] += board[i];
        board[i] = 0;
    }
}

void declare_winner()
{
    printf("Final Scores:\n");
    printf("Player 1: %d\n", scores[0]);
    printf("Player 2: %d\n", scores[1]);
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

void game_loop()
{
    init_game();
    while (!is_game_over())
    {
        display_board();
        play_turn(current_player);
        switch_player();
    }
    collect_remaining_seeds();
    declare_winner();
}
