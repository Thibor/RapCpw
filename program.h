#pragma once

#include <cassert>
#include <iostream>
#include <stdio.h>
#include <string>
#include "windows.h"

#define INF 10000
#define INVALID 32767
#define MAX_DEPTH 100

#define U64 unsigned __int64
#define U32 unsigned __int32
#define U16 unsigned __int16
#define U8  unsigned __int8
#define S64 signed   __int64
#define S32 signed   __int32
#define S16 signed   __int16
#define S8  signed   __int8
#define SQ  unsigned __int8

/* Move ordering constants */

#define SORT_KING 400000000
#define SORT_HASH 200000000
#define SORT_CAPT 100000000
#define SORT_PROM  90000000
#define SORT_KILL  80000000
#define STARTFEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

enum epiece {
	KING,
	QUEEN,
	ROOK,
	BISHOP,
	KNIGHT,
	PAWN,
	PIECE_EMPTY
};

enum ecolor {
	WHITE,
	BLACK,
	COLOR_EMPTY
};

enum esqare {
	A1 = 0, B1, C1, D1, E1, F1, G1, H1,
	A2 = 16, B2, C2, D2, E2, F2, G2, H2,
	A3 = 32, B3, C3, D3, E3, F3, G3, H3,
	A4 = 48, B4, C4, D4, E4, F4, G4, H4,
	A5 = 64, B5, C5, D5, E5, F5, G5, H5,
	A6 = 80, B6, C6, D6, E6, F6, G6, H6,
	A7 = 96, B7, C7, D7, E7, F7, G7, H7,
	A8 = 112, B8, C8, D8, E8, F8, G8, H8
};

enum ecastle {
	CASTLE_WK = 1,
	CASTLE_WQ = 2,
	CASTLE_BK = 4,
	CASTLE_BQ = 8
};

enum emflag {
	MFLAG_NORMAL = 0,
	MFLAG_CAPTURE = 1,
	MFLAG_EPCAPTURE = 2,
	MFLAG_CASTLE = 4,
	MFLAG_EP = 8,
	MFLAG_PROMOTION = 16,
	MFLAG_NULLMOVE = 32
};

struct sboard {
	U8	 pieces[128];
	U8	 color[128];
	char stm;        // side to move: 0 = white,  1 = black
	char castle;     // 1 = shortW, 2 = longW, 4 = shortB, 8 = longB
	char ep;         // en passant square
	U8   ply;
	U64  hash;
	U64	 phash;
	int  rep_index;
	U64  rep_stack[1024];
	S8   king_loc[2];
	int  pcsq_mg[2];
	int  pcsq_eg[2];
	int piece_material[2];
	int pawn_material[2];
	U8 piece_cnt[2][6];
	U8 pawns_on_file[2][8];
	U8 pawns_on_rank[2][8];
	U8 pawn_ctrl[2][128];
};
extern sboard board;


struct smove {
	char id;
	SQ from;
	SQ to;
	U8 piece_from;
	U8 piece_to;
	U8 piece_cap;
	char flags;
	char castle;
	char ply;
	char ep;
	int score;
};


struct sSearchDriver {
	int myside;
	U8 depth;
	int history[2][128][128];
	int cutoff[2][128][128];
	smove killers[1024][2];
	char pv[2048];
	S16 score;
	U64 nodes;
	S32 movetime;
	U64 q_nodes;
	U64 starttime;
};
extern sSearchDriver sd;

enum etimef {
	FTIME = 0x1,
	FINC = 0x2,
	FMOVESTOGO = 0x4,
	FDEPTH = 0x8,
	FNODES = 0x10,
	FMATE = 0x20,
	FMOVETIME = 0x40,
	FINFINITE = 0x80
};

struct schronos {
	bool ponder;
	bool post;
	bool timeOver;
	int time[2];
	int inc[2];
	int movestogo;
	int depth;
	int nodes;
	int mate;
	int movetime;
	U8 flags;
};
extern schronos chronos;

struct s_options
{
	bool ponder = true;
	int contempt = 10;
	int aspiration = 50;  // size of the aspiration window ( val-ASPITATION, val+ASPIRATION )
	int elo = 2500;
	int eloMin = 1000;
	int eloMax = 2500;
};
extern s_options options;

struct s_eval_data
{
	int PIECE_VALUE[6];
	int SORT_VALUE[6];

	/* Piece-square tables - we use size of the board representation,
	not 0..63, to avoid re-indexing. Initialization routine, however,
	uses 0..63 format for clarity */
	int mgPst[6][2][128];
	int egPst[6][2][128];

	/* piece-square tables for pawn structure */

	int weak_pawn[2][128]; // isolated and backward pawns are scored in the same way
	int passed_pawn[2][128];
	int protected_passer[2][128];

	int sqNearK[2][128][128];

	/* single values - letter p before a name signifies a penalty */

	int BISHOP_PAIR;
	int P_KNIGHT_PAIR;
	int P_ROOK_PAIR;
	int ROOK_OPEN;
	int ROOK_HALF;
	int P_BISHOP_TRAPPED_A7;
	int P_BISHOP_TRAPPED_A6;
	int P_KNIGHT_TRAPPED_A8;
	int P_KNIGHT_TRAPPED_A7;
	int P_BLOCK_CENTRAL_PAWN;
	int P_KING_BLOCKS_ROOK;

	int SHIELD_2;
	int SHIELD_3;
	int P_NO_SHIELD;

	int RETURNING_BISHOP;
	int P_C3_KNIGHT;
	int P_NO_FIANCHETTO;
	int FIANCHETTO;
	int TEMPO;
	int ENDGAME_MAT;
};
extern s_eval_data e;

extern char vector[5][8];
extern bool slide[5];
extern char vectors[5];

void board_display();
void clearBoard();
void FillSq(U8 color, U8 piece, S8 sq);
void ClearSq(SQ sq);
int board_loadFromFen(char* fen);

void Bestmove();
void UciCommand(char* command);
void CheckInput();

U8 movegen(smove* moves, U8 tt_move);
U8 movegen_qs(smove* moves);
void movegen_sort(U8 movecount, smove* m, U8 current);


void convert_0x88_a(SQ sq, char* a);
SQ convert_a_0x88(char* a);
char* algebraic_writemove(smove m, char* a);
bool algebraic_moves(char* a);


int move_make(smove move);
int move_unmake(smove move);
int move_makeNull();
int move_unmakeNull(char ep);

// the next couple of functions respond to questions about moves or move lists

int move_iscapt(smove m);
int move_isprom(smove m);
int move_canSimplify(smove m);
int move_countLegal();
bool move_isLegal(smove m);


smove strToMove(char* a);


void search_run(); // interface of the search functions
void clearHistoryTable();


void setDefaultEval();
void setBasicValues();
void setSquaresNearKing();
void setPcsq();
void correctValues();
void readIniFile();
void processIniString(char line[250]);


int eval(int alpha, int beta, int use_hash);
int isPiece(U8 color, U8 piece, SQ sq);
int getTropism(int sq1, int sq2);
void PrintBoard();
void printEval();
void printEvalFactor(int wh, int bl);


int Quiesce(int alpha, int beta);
bool badCapture(smove move);
bool Blind(smove move);

bool isAttacked(char byColor, SQ sq);
bool leaperAttack(char byColor, SQ sq, char byPiece);
bool straightAttack(char byColor, SQ sq, int vect);
bool diagAttack(int byColor, SQ sq, int vect);
bool bishAttack(int byColor, SQ sq, int vect);

void util_perft(char* command);
U64 perft(U8 depth);

void util_bench(char* command);
void GetPv(char* pv);

unsigned int gettime();
void time_calc_movetime();
bool time_stop_root();
bool time_stop();
bool isRepetition();
void printSearchHeader();
void printStats(unsigned int time,unsigned long long nodes,unsigned long long qnodes);

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

/* pattern detection */
void blockedPieces(int side);

//uci
void UciLoop();
//input
bool GetInput(std::string& s);
int InputInit();
//movegen
void movegen_push(char from, char to, U8 piece_from, U8 piece_cap, char flags);
void movegen_push_qs(char from, char to, U8 piece_from, U8 piece_cap, char flags);
void movegen_pawn_move(SQ sq, bool promotion_only);
void movegen_pawn_capt(SQ sq);
//search
void search_iterate();
int search_widen(int depth, int val);
void search_clearDriver();
int search_root(U8 depth, int alpha, int beta);
int Search(U8 depth, U8 ply, int alpha, int beta, int can_null, int is_pv);
void setKillers(smove m, U8 ply);
void ReorderMoves(smove* m, U8 mcount, U8 ply);
void info_pv(int val);
unsigned int countNps(unsigned int nodes, unsigned int time);
void ageHistoryTable();
int Contempt();

//transposition
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
extern class spawntt_entry* ptt;

struct sevaltt_entry {
	U64 hash;
	int val;
};
extern sevaltt_entry* ett;

extern U64 tt_size;
extern int ptt_size;
extern int ett_size;

U64 rand64();
int tt_init();
int tt_setsize(int size);
int tt_probe(U8 depth, int alpha, int beta, char* best);
void tt_save(U8 depth, int val, char flags, char best);
int ttpawn_setsize(int size);
int ttpawn_probe();
void ttpawn_save(int val);
int tteval_setsize(int size);
int tteval_probe();
void tteval_save(int val);
U64 ttPermill();
//board
/* row identifiers */
#define ROW_1   ( A1 >> 4 )
#define ROW_2   ( A2 >> 4 )
#define ROW_3   ( A3 >> 4 )
#define ROW_4   ( A4 >> 4 )
#define ROW_5   ( A5 >> 4 )
#define ROW_6   ( A6 >> 4 )
#define ROW_7   ( A7 >> 4 )
#define ROW_8   ( A8 >> 4 )

/* column identifiers */
#define COL_A  ( A1 & 7 )
#define COL_B  ( B1 & 7 )
#define COL_C  ( C1 & 7 )
#define COL_D  ( D1 & 7 )
#define COL_E  ( E1 & 7 )
#define COL_F  ( F1 & 7 )
#define COL_G  ( G1 & 7 )
#define COL_H  ( H1 & 7 )

/* vectors */
#define NORTH  16
#define NN    ( NORTH + NORTH )
#define SOUTH  -16
#define SS    ( SOUTH + SOUTH )
#define EAST  1
#define WEST  -1
#define NE    17
#define SW    -17
#define NW    15
#define SE    -15

/* generate square number from row and column */
#define SET_SQ(row,col) (row * 16 + col)

/* does a given number represent a square on the board? */
#define IS_SQ(x)  ( (x) & 0x88 ) ? (0) : (1)

/* get board column that a square is part of */
#define COL(sq)  ( (sq) & 7 )

/* get board row that a square is part of */
#define ROW(sq)  ( (sq) >> 4 )

/* determine if two squares lie on the same column */
#define SAME_COL(sq1,sq2) ( ( COL(sq1) == COL(sq2) ) ? (1) : (0) )

/* determine if two squares lie in the same row */
#define SAME_ROW(sq1,sq2) ( ( ROW(sq1) == ROW(sq2) ) ? (1) : (0) )