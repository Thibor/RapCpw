#pragma once

#include "board.h"
#include "program.h"
#include "transposition.h"

/* king safety*/
int wKingShield();
int bKingShield();

/* pawn structure */
int getPawnScore();
int evalPawnStructure();
int EvalPawn(SQ sq, S8 side);
void EvalKnight(SQ sq, S8 side);
void EvalBishop(SQ sq, S8 side);
void EvalRook(SQ sq, S8 side);
void EvalQueen(SQ sq, S8 side);
bool isPawnSupported(SQ sq, S8 side);
int isWPSupported(SQ sq);
int isBPSupported(SQ sq);

/* pattern detection */
void blockedPieces(int side);
