#include "../../include/game.h"
#include <stdio.h>
#include <stdlib.h>



int main () {
    game_loop();
    return 0;
}

void init_game() {
    for (int i = 0; i < 12; i++) {
        board[i] = 4;
    }
    scores[0] = 0;
    scores[1] = 0;
    current_player = 0;
    game_over = 0;
}

void display_board() {
    printf("\n");
    printf("      [11][10][9][8][7][6]\n");
    printf("      ");
    for (int i = 11; i >= 6; i--) {
        printf(" %2d ", board[i]);
    }
    printf("\n");
    printf("P2 %2d                   %2d P1\n", scores[1], scores[0]);
    printf("      ");
    for (int i = 0; i <= 5; i++) {
        printf(" %2d ", board[i]);
    }
    printf("\n");
    printf("      [0] [1] [2] [3] [4] [5]\n");
    printf("\n");
}

char play_turn(char player, char last_pit_index) {
    int pit_index;
    if (last_pit_index != -1) {
    char new_score = collect_seeds(player, last_pit_index);
    scores[player] += new_score;
    display_board();
    }

    printf("Player %d's turn. Choose a pit (0-5 for P1, 6-11 for P2): ", player + 1);
    scanf("%d", &pit_index);
    move(player, pit_index);
}

char collect_seeds(char player, char last_pit_index) {
    if (board[last_pit_index] == 2 || board[last_pit_index] == 3) {
        char collected = board[last_pit_index];
        board[last_pit_index] = 0;
        return collected + collect_seeds(player, (last_pit_index - 1 + 12) % 12);
    }
    return 0;
}

void move(char player, char pit_index) {
    if ((player == 0 && (pit_index < 0 || pit_index > 5)) ||
        (player == 1 && (pit_index < 6 || pit_index > 11)) ||
        board[pit_index] == 0) {
        printf("Invalid move. Try again.\n");
        
        return;
    }

    int seeds = board[pit_index];
    board[pit_index] = 0;
    int index = pit_index;

    while (seeds > 0) {
        index = (index + 1) % 12;
        board[index]++;
        seeds--;
    }

    // Capture logic can be added here
}

void switch_player() {
    current_player = 1 - current_player;
}

char is_game_over() {

    //TODO: implement more conditions for game over
    int side1_empty = 1;
    int side2_empty = 1;

    for (int i = 0; i < 6; i++) {
        if (board[i] > 0) {
            side1_empty = 0;
            break;
        }
    }

    for (int i = 6; i < 12; i++) {
        if (board[i] > 0) {
            side2_empty = 0;
            break;
        }
    }

    if (side1_empty || side2_empty) {
        game_over = 1;
    }

    return game_over;
}

void game_loop() {
    init_game();
    char last_pit_index = -1;
    while (!is_game_over()) {
        display_board();
        last_pit_index = play_turn(current_player, last_pit_index);
        switch_player();
    }
    collect_remaining_seeds();
    declare_winner();
}


