#pragma once

#include "program.h"

struct szobrist {
    U64 piecesquare[6][2][128];
    U64 color;
    U64 castling[16];
    U64 ep[128];
};
extern szobrist zobrist;

enum ettflag {
    TT_EXACT,
    TT_ALPHA,
    TT_BETA
};

struct stt_entry {
    U64  hash;
    int  val;
    U8	 depth;
    U8   flags;
    U8   bestmove;
};
extern stt_entry* tt;

struct spawntt_entry {
    U64  hash;
    int  val;
};
extern class spawntt_entry *ptt;

struct sevaltt_entry {
    U64 hash;
    int val;
};
extern sevaltt_entry *ett;

extern U64 tt_size;
extern int ptt_size;
extern int ett_size;

U64 rand64();
int tt_init();
int tt_setsize(int size);
int tt_probe(U8 depth, int alpha, int beta, char * best);
void tt_save(U8 depth, int val, char flags, char best);
int ttpawn_setsize(int size);
int ttpawn_probe();
void ttpawn_save(int val);
int tteval_setsize(int size);
int tteval_probe();
void tteval_save(int val);
U64 ttPermill();
