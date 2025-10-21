/*************************************************************************
                           Awale -- Game
                             -------------------
    début                : 21/10/2025
    copyright            : (C) 2025 par Mohamed et Diego
    e-mail               : mohamed.lemseffer@insa-lyon.fr / diego.aquinoh@insa-lyon.fr / 

*************************************************************************/

//---------- Interface of the <Game> (file Game.h) ----------------
#if ! defined ( Game_H )
#define Game_H
#include <stdio.h>
#include <stdlib.h>

//--------------------------------------------------- Interfaces used
using namespace std;

//------------------------------------------------------------- Constantes
//------------------------------------------------------------------ Types

//-------------------------------- Declarations



char board[12];


char collected = 0;

void init_game();

void display_board();
char is_game_over();
void collect_remaining_seeds();
void declare_winner();

void play_turn(char player);
void move(char player, char pit_index);


#endif // Game_H
