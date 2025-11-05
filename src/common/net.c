#include "../../include/net.h"

int apply_move_from_pit(int player, int pit_index) {
    if ((player == 0 && (pit_index < 0 || pit_index > 5)) ||
        (player == 1 && (pit_index < 6 || pit_index > 11)) ||
        board[pit_index] == 0) {
        return -2;
    }
    int seeds = board[pit_index];
    board[pit_index] = 0;
    int index = pit_index;
    while (seeds > 0) {
        index = (index + 1) % 12;
        if (index != pit_index) {
            board[index]++;
            seeds--;
        }
    }
    return index;
}
