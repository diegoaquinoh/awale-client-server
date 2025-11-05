/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 21/10/2025
    copyright            : (C) 2025 par Mohamed et Diego
    e-mail               : mohamed.lemseffer@insa-lyon.fr / diego.aquinoh@insa-lyon.fr / 

*************************************************************************/

//---------- Interface of the <Game> (file Game.h) ----------------
#ifndef GAME_H
#define GAME_H
#include <stdio.h>
#include <stdlib.h>

extern char board[12];
extern char scores[2];
extern char current_player;
extern char game_over;

void init_game();
void display_board();
char is_game_over(char continued);
void collect_remaining_seeds(char continued);
void declare_winner(char continued);
char collect_seeds(char player, char last_pit_index);
char play_turn(char player);
char move(char player);
void switch_player();
char proposition_draw(char player);
void game_loop();

#endif // GAME_H

