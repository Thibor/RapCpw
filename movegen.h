#pragma once

#include "program.h"
#include "board.h"
#include "program.h"
#include "transposition.h"

/*********************************************************************
*                              CPW-engine                            *
*           created by some members of Chessprogramming Wiki         *
*                                                                    *
* movegen.h - this file names functions coded within movegen.cpp.    *
* Only movegen() and movegen_sort() are used outside that file.      *
*********************************************************************/

void movegen_push(char from, char to, U8 piece_from, U8 piece_cap, char flags);
void movegen_push_qs(char from, char to, U8 piece_from, U8 piece_cap, char flags);
void movegen_pawn_move(SQ sq, bool promotion_only);
void movegen_pawn_capt(SQ sq);