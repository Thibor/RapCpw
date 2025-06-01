#include "program.h"

using namespace std;

#pragma region variable

/******************************************************************************
*  We want our eval to be color-independent, i.e. the same functions ought to *
*  be called for white and black pieces. This requires some way of converting *
*  row and square coordinates.                                                *
******************************************************************************/

static const int seventh[2] = { ROW(A7), ROW(A2) };
static const int eighth[2] = { ROW(A8), ROW(A1) };
static const int stepFwd[2] = { NORTH, SOUTH };
static const int stepBck[2] = { SOUTH, NORTH };

static const int inv_sq[128] = {
		A8, B8, C8, D8, E8, F8, G8, H8, -1, -1, -1, -1, -1, -1, -1, -1,
		A7, B7, C7, D7, E7, F7, G7, H7, -1, -1, -1, -1, -1, -1, -1, -1,
		A6, B6, C6, D6, E6, F6, G6, H6, -1, -1, -1, -1, -1, -1, -1, -1,
		A5, B5, C5, D5, E5, F5, G5, H5, -1, -1, -1, -1, -1, -1, -1, -1,
		A4, B4, C4, D4, E4, F4, G4, H4, -1, -1, -1, -1, -1, -1, -1, -1,
		A3, B3, C3, D3, E3, F3, G3, H3, -1, -1, -1, -1, -1, -1, -1, -1,
		A2, B2, C2, D2, E2, F2, G2, H2, -1, -1, -1, -1, -1, -1, -1, -1,
		A1, B1, C1, D1, E1, F1, G1, H1, -1, -1, -1, -1, -1, -1, -1, -1
};

#define REL_SQ(cl, sq)       ((cl) == (WHITE) ? (sq) : (inv_sq[sq]))

/* adjustements of piece value based on the number of own pawns */
int n_adj[9] = { -20, -16, -12, -8, -4,  0,  4,  8, 12 };
int r_adj[9] = { 15,  12,   9,  6,  3,  0, -3, -6, -9 };

static const int SafetyTable[100] = {
	 0,  0,   1,   2,   3,   5,   7,   9,  12,  15,
	18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
	68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
	140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
	260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
	377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
	494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

/******************************************************************************
*  This struct holds data about certain aspects of evaluation, which allows   *
*  our program to print them if desired.                                      *
******************************************************************************/

struct eval_vector {
	int gamePhase;   // function of piece material: 24 in opening, 0 in endgame
	int mgMob[2];     // midgame mobility
	int egMob[2];     // endgame mobility
	int attCnt[2];    // no. of pieces attacking zone around enemy king
	int attWeight[2]; // weight of attacking pieces - index to SafetyTable
	int mgTropism[2]; // midgame king tropism score
	int egTropism[2]; // endgame king tropism score
	int kingShield[2];
	int adjustMaterial[2];
	int blockages[2];
	int positionalThemes[2];
} v;

s_eval_data e;

// tables used for translating piece/square tables to internal 0x88 representation

int index_white[64] = {
	A8, B8, C8, D8, E8, F8, G8, H8,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A1, B1, C1, D1, E1, F1, G1, H1
};

int index_black[64] = {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8
};

/******************************************************************************
*                           PAWN PCSQ                                         *
*                                                                             *
*  Unlike TSCP, CPW generally doesn't want to advance its pawns. Its piece/   *
*  square table for pawns takes into account the following factors:           *
*                                                                             *
*  - file-dependent component, encouraging program to capture                 *
*    towards the center                                                       *
*  - small bonus for staying on the 2nd rank                                  *
*  - small bonus for standing on a3/h3                                        *
*  - penalty for d/e pawns on their initial squares                           *
*  - bonus for occupying the center                                           *
******************************************************************************/

int org_pawn_pcsq_mg[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
	-6,  -4,   1,   1,   1,   1,  -4,  -6,
	-6,  -4,   1,   2,   2,   1,  -4,  -6,
	-6,  -4,   2,   8,   8,   2,  -4,  -6,
	-6,  -4,   5,  10,  10,   5,  -4,  -6,
	-4,  -4,   1,   5,   5,   1,  -4,  -4,
	-6,  -4,   1, -24,  -24,  1,  -4,  -6,
	 0,   0,   0,   0,   0,   0,   0,   0
};

int pawn_pcsq_eg[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
	-6,  -4,   1,   1,   1,   1,  -4,  -6,
	-6,  -4,   1,   2,   2,   1,  -4,  -6,
	-6,  -4,   2,   8,   8,   2,  -4,  -6,
	-6,  -4,   5,  10,  10,   5,  -4,  -6,
	-4,  -4,   1,   5,   5,   1,  -4,  -4,
	-6,  -4,   1, -24,  -24,  1,  -4,  -6,
	 0,   0,   0,   0,   0,   0,   0,   0
};
int pawn_pcsq_mg[64] = {};

/******************************************************************************
*    KNIGHT PCSQ                                                              *
*                                                                             *
*   - centralization bonus                                                    *
*   - rim and back rank penalty, including penalty for not being developed    *
******************************************************************************/

int org_knight_pcsq_mg[64] = {
	-8,  -8,  -8,  -8,  -8,  -8,  -8,  -8,
	-8,   0,   0,   0,   0,   0,   0,  -8,
	-8,   0,   4,   6,   6,   4,   0,  -8,
	-8,   0,   6,   8,   8,   6,   0,  -8,
	-8,   0,   6,   8,   8,   6,   0,  -8,
	-8,   0,   4,   6,   6,   4,   0,  -8,
	-8,   0,   1,   2,   2,   1,   0,  -8,
   -16, -12,  -8,  -8,  -8,  -8, -12,  -16
};

int knight_pcsq_eg[64] = {
	-8,  -8,  -8,  -8,  -8,  -8,  -8,  -8,
	-8,   0,   0,   0,   0,   0,   0,  -8,
	-8,   0,   4,   6,   6,   4,   0,  -8,
	-8,   0,   6,   8,   8,   6,   0,  -8,
	-8,   0,   6,   8,   8,   6,   0,  -8,
	-8,   0,   4,   6,   6,   4,   0,  -8,
	-8,   0,   1,   2,   2,   1,   0,  -8,
   -16, -12,  -8,  -8,  -8,  -8, -12,  -16
};
int knight_pcsq_mg[64] = {};
/******************************************************************************
*                BISHOP PCSQ                                                  *
*                                                                             *
*   - centralization bonus, smaller than for knight                           *
*   - penalty for not being developed                                         *
*   - good squares on the own half of the board                               *
******************************************************************************/

int org_bishop_pcsq_mg[64] = {
	-4,  -4,  -4,  -4,  -4,  -4,  -4,  -4,
	-4,   0,   0,   0,   0,   0,   0,  -4,
	-4,   0,   2,   4,   4,   2,   0,  -4,
	-4,   0,   4,   6,   6,   4,   0,  -4,
	-4,   0,   4,   6,   6,   4,   0,  -4,
	-4,   1,   2,   4,   4,   2,   1,  -4,
	-4,   2,   1,   1,   1,   1,   2,  -4,
	-4,  -4, -12,  -4,  -4, -12,  -4,  -4
};

int bishop_pcsq_eg[64] = {
	-4,  -4,  -4,  -4,  -4,  -4,  -4,  -4,
	-4,   0,   0,   0,   0,   0,   0,  -4,
	-4,   0,   2,   4,   4,   2,   0,  -4,
	-4,   0,   4,   6,   6,   4,   0,  -4,
	-4,   0,   4,   6,   6,   4,   0,  -4,
	-4,   1,   2,   4,   4,   2,   1,  -4,
	-4,   2,   1,   1,   1,   1,   2,  -4,
	-4,  -4, -12,  -4,  -4, -12,  -4,  -4
};
int bishop_pcsq_mg[64] = {};
/******************************************************************************
*                        ROOK PCSQ                                            *
*                                                                             *
*    - bonus for 7th and 8th ranks                                            *
*    - penalty for a/h columns                                                *
*    - small centralization bonus                                             *
******************************************************************************/

int org_rook_pcsq_mg[64] = {
	 5,   5,   5,   5,   5,   5,   5,   5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	 0,   0,   0,   2,   2,   0,   0,   0
};

int rook_pcsq_eg[64] = {
	 5,   5,   5,   5,   5,   5,   5,   5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	 0,   0,   0,   2,   2,   0,   0,   0
};
int rook_pcsq_mg[64] = {};
/******************************************************************************
*                     QUEEN PCSQ                                              *
*                                                                             *
* - small bonus for centralization in the endgame                             *
* - penalty for staying on the 1st rank, between rooks in the midgame         *
******************************************************************************/

int org_queen_pcsq_mg[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
	 0,   0,   1,   1,   1,   1,   0,   0,
	 0,   0,   1,   2,   2,   1,   0,   0,
	 0,   0,   2,   3,   3,   2,   0,   0,
	 0,   0,   2,   3,   3,   2,   0,   0,
	 0,   0,   1,   2,   2,   1,   0,   0,
	 0,   0,   1,   1,   1,   1,   0,   0,
	-5,  -5,  -5,  -5,  -5,  -5,  -5,  -5
};

int queen_pcsq_eg[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
	 0,   0,   1,   1,   1,   1,   0,   0,
	 0,   0,   1,   2,   2,   1,   0,   0,
	 0,   0,   2,   3,   3,   2,   0,   0,
	 0,   0,   2,   3,   3,   2,   0,   0,
	 0,   0,   1,   2,   2,   1,   0,   0,
	 0,   0,   1,   1,   1,   1,   0,   0,
	-5,  -5,  -5,  -5,  -5,  -5,  -5,  -5
};
int queen_pcsq_mg[64] = {};

int king_pcsq_mg[64] = {
   -40, -30, -50, -70, -70, -50, -30, -40,
   -30, -20, -40, -60, -60, -40, -20, -30,
   -20, -10, -30, -50, -50, -30, -10, -20,
   -10,   0, -20, -40, -40, -20,   0, -10,
	 0,  10, -10, -30, -30, -10,  10,   0,
	10,  20,   0, -20, -20,   0,  20,  10,
	30,  40,  20,   0,   0,  20,  40,  30,
	40,  50,  30,  10,  10,  30,  50,  40
};

int king_pcsq_eg[64] = {
   -72, -48, -36, -24, -24, -36, -48, -72,
   -48, -24, -12,   0,   0, -12, -24, -48,
   -36, -12,   0,  12,  12,   0, -12, -36,
   -24,   0,  12,  24,  24,  12,   0, -24,
   -24,   0,  12,  24,  24,  12,   0, -24,
   -36, -12,   0,  12,  12,   0, -12, -36,
   -48, -24, -12,   0,   0, -12, -24, -48,
   -72, -48, -36, -24, -24, -36, -48, -72
};

/******************************************************************************
*                     WEAK PAWNS PCSQ                                         *
*                                                                             *
*  Current version of CPW-engine does not differentiate between isolated and  *
*  backward pawns, using one  generic  cathegory of  weak pawns. The penalty  *
*  is bigger in the center, on the assumption that weak central pawns can be  *
*  attacked  from many  directions. If the penalty seems too low, please note *
*  that being on a semi-open file will come into equation, too.               *
******************************************************************************/

int weak_pawn_pcsq[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
   -10, -12, -14, -16, -16, -14, -12, -10,
   -10, -12, -14, -16, -16, -14, -12, -10,
   -10, -12, -14, -16, -16, -14, -12, -10,
   -10, -12, -14, -16, -16, -14, -12, -10,
   -10, -12, -14, -16, -16, -14, -12, -10,
   -10, -12, -14, -16, -16, -14, -12, -10,
	 0,   0,   0,   0,   0,   0,   0,   0
};

int passed_pawn_pcsq[64] = {
	 0,   0,   0,   0,   0,   0,   0,   0,
   140, 140, 140, 140, 140, 140, 140, 140,
	92,  92,  92,  92,  92,  92,  92,  92,
	56,  56,  56,  56,  56,  56,  56,  56,
	32,  32,  32,  32,  32,  32,  32,  32,
	20,  20,  20,  20,  20,  20,  20,  20,
	20,  20,  20,  20,  20,  20,  20,  20,
	 0,   0,   0,   0,   0,   0,   0,   0
};

int material[6] = { 0,975,500,335,325,100 };
#pragma endregion

int EloRnd(int range) {
	return rand() % (range + 1) - (range >> 1);
}

static void SetElo() {
	srand(time(NULL));
	int elo = options.elo;
	if (elo < options.eloMin)
		elo = options.eloMin;
	if (elo > options.eloMax)
		elo = options.eloMax;
	elo -= options.eloMin;
	int eloRange = options.eloMax - options.eloMin;
	int range = 800 - (elo * 800) / eloRange;
	for (int n = 0; n < 64; n++) {
		pawn_pcsq_mg[n] = org_pawn_pcsq_mg[n] + EloRnd(range);
		knight_pcsq_mg[n] = org_knight_pcsq_mg[n] + EloRnd(range);
		bishop_pcsq_mg[n] = org_bishop_pcsq_mg[n] + EloRnd(range);
		rook_pcsq_mg[n] = org_rook_pcsq_mg[n] + EloRnd(range);
		queen_pcsq_mg[n] = org_queen_pcsq_mg[n] + EloRnd(range);
	}
}

void setDefaultEval()
{
	SetElo();
	setBasicValues();
	setSquaresNearKing();
	setPcsq();
	readIniFile();
	correctValues();
}

void setBasicValues() {

	/********************************************************************************
	*  We use material values by IM Larry Kaufman with additional + 10 for a Bishop *
	*  and only +30 for a Bishop pair 	                                            *
	********************************************************************************/
	for (int n = 0; n < 6; n++)
		e.PIECE_VALUE[n] = material[n];
	/*e.PIECE_VALUE[KING] = 0;
	e.PIECE_VALUE[QUEEN] = 975;
	e.PIECE_VALUE[ROOK] = 500;
	e.PIECE_VALUE[BISHOP] = 335;
	e.PIECE_VALUE[KNIGHT] = 325;
	e.PIECE_VALUE[PAWN] = 100;*/

	e.BISHOP_PAIR = 30;
	e.P_KNIGHT_PAIR = 8;
	e.P_ROOK_PAIR = 16;

	/*************************************************
	* Values used for sorting captures are the same  *
	* as normal piece values, except for a king.     *
	*************************************************/

	for (int i = 0; i < 6; ++i) {
		e.SORT_VALUE[i] = e.PIECE_VALUE[i];
	}
	e.SORT_VALUE[KING] = SORT_KING;

	/* trapped and blocked pieces */
	e.P_KING_BLOCKS_ROOK = 24;
	e.P_BLOCK_CENTRAL_PAWN = 24;
	e.P_BISHOP_TRAPPED_A7 = 150;
	e.P_BISHOP_TRAPPED_A6 = 50;
	e.P_KNIGHT_TRAPPED_A8 = 150;
	e.P_KNIGHT_TRAPPED_A7 = 100;

	/* minor penalties */
	e.P_C3_KNIGHT = 5;
	e.P_NO_FIANCHETTO = 4;

	/* king's defence */
	e.SHIELD_2 = 10;
	e.SHIELD_3 = 5;
	e.P_NO_SHIELD = 10;

	/* minor bonuses */
	e.ROOK_OPEN = 10;
	e.ROOK_HALF = 5;
	e.RETURNING_BISHOP = 20;
	e.FIANCHETTO = 4;
	e.TEMPO = 10;

	e.ENDGAME_MAT = 1300;
}

void setSquaresNearKing() {
	for (int i = 0; i < 128; ++i)
		for (int j = 0; j < 128; ++j)
		{

			e.sqNearK[WHITE][i][j] = 0;
			e.sqNearK[BLACK][i][j] = 0;

			if (IS_SQ(i) && IS_SQ(j)) {

				/* squares constituting the ring around both kings */
				if (j == i + NORTH || j == i + SOUTH
					|| j == i + EAST || j == i + WEST
					|| j == i + NW || j == i + NE
					|| j == i + SW || j == i + SE) {

					e.sqNearK[WHITE][i][j] = 1;
					e.sqNearK[BLACK][i][j] = 1;
				}

				/* squares in front of the white king ring */
				if (j == i + NORTH + NORTH
					|| j == i + NORTH + NE
					|| j == i + NORTH + NW)
					e.sqNearK[WHITE][i][j] = 1;

				/* squares in front og the black king ring */
				if (j == i + SOUTH + SOUTH
					|| j == i + SOUTH + SE
					|| j == i + SOUTH + SW)
					e.sqNearK[WHITE][i][j] = 1;
			}
		}
}


void setPcsq() {

	for (int i = 0; i < 64; ++i) {

		e.weak_pawn[WHITE][index_white[i]] = weak_pawn_pcsq[i];
		e.weak_pawn[BLACK][index_black[i]] = weak_pawn_pcsq[i];
		e.passed_pawn[WHITE][index_white[i]] = passed_pawn_pcsq[i];
		e.passed_pawn[BLACK][index_black[i]] = passed_pawn_pcsq[i];

		/* protected passers are slightly stronger than ordinary passers */

		e.protected_passer[WHITE][index_white[i]] = (passed_pawn_pcsq[i] * 10) / 8;
		e.protected_passer[BLACK][index_black[i]] = (passed_pawn_pcsq[i] * 10) / 8;

		/* now set the piece/square tables for each color and piece type */

		e.mgPst[PAWN][WHITE][index_white[i]] = pawn_pcsq_mg[i];
		e.mgPst[PAWN][BLACK][index_black[i]] = pawn_pcsq_mg[i];
		e.mgPst[KNIGHT][WHITE][index_white[i]] = knight_pcsq_mg[i];
		e.mgPst[KNIGHT][BLACK][index_black[i]] = knight_pcsq_mg[i];
		e.mgPst[BISHOP][WHITE][index_white[i]] = bishop_pcsq_mg[i];
		e.mgPst[BISHOP][BLACK][index_black[i]] = bishop_pcsq_mg[i];
		e.mgPst[ROOK][WHITE][index_white[i]] = rook_pcsq_mg[i];
		e.mgPst[ROOK][BLACK][index_black[i]] = rook_pcsq_mg[i];
		e.mgPst[QUEEN][WHITE][index_white[i]] = queen_pcsq_mg[i];
		e.mgPst[QUEEN][BLACK][index_black[i]] = queen_pcsq_mg[i];
		e.mgPst[KING][WHITE][index_white[i]] = king_pcsq_mg[i];
		e.mgPst[KING][BLACK][index_black[i]] = king_pcsq_mg[i];

		e.egPst[PAWN][WHITE][index_white[i]] = pawn_pcsq_eg[i];
		e.egPst[PAWN][BLACK][index_black[i]] = pawn_pcsq_eg[i];
		e.egPst[KNIGHT][WHITE][index_white[i]] = knight_pcsq_eg[i];
		e.egPst[KNIGHT][BLACK][index_black[i]] = knight_pcsq_eg[i];
		e.egPst[BISHOP][WHITE][index_white[i]] = bishop_pcsq_eg[i];
		e.egPst[BISHOP][BLACK][index_black[i]] = bishop_pcsq_eg[i];
		e.egPst[ROOK][WHITE][index_white[i]] = rook_pcsq_eg[i];
		e.egPst[ROOK][BLACK][index_black[i]] = rook_pcsq_eg[i];
		e.egPst[QUEEN][WHITE][index_white[i]] = queen_pcsq_eg[i];
		e.egPst[QUEEN][BLACK][index_black[i]] = queen_pcsq_eg[i];
		e.egPst[KING][WHITE][index_white[i]] = king_pcsq_eg[i];
		e.egPst[KING][BLACK][index_black[i]] = king_pcsq_eg[i];
	}
}

/* This function is meant to be used in conjunction with the *.ini file.
Its aim is to make sure that all the assumptions made within the program
are met.  */

void correctValues() {
	if (e.PIECE_VALUE[BISHOP] == e.PIECE_VALUE[KNIGHT])
		++e.PIECE_VALUE[BISHOP];
}

void readIniFile()
{
	char line[256];
	FILE* cpw_init = fopen("cpw.ini", "r");
	if (!cpw_init)
		return;
	while (fgets(line, 250, cpw_init))
	{
		if (line[0] == ';') continue; // don't process comment lines
		processIniString(line);
	}
}

void processIniString(char line[250]) {
	int converted;
	/* piece values */
	if (!strncmp(line, "PAWN_VALUE", 10))
		converted = sscanf(line, "PAWN_VALUE %d", &e.PIECE_VALUE[PAWN]);
	else if (!strncmp(line, "KNIGHT_VALUE", 12))
		converted = sscanf(line, "KNIGHT_VALUE %d", &e.PIECE_VALUE[KNIGHT]);
	else if (!strncmp(line, "BISHOP_VALUE", 12))
		converted = sscanf(line, "BISHOP_VALUE %d", &e.PIECE_VALUE[BISHOP]);
	else if (!strncmp(line, "ROOK_VALUE", 10))
		converted = sscanf(line, "ROOK_VALUE %d", &e.PIECE_VALUE[ROOK]);
	else if (!strncmp(line, "QUEEN_VALUE", 11))
		converted = sscanf(line, "QUEEN_VALUE %d", &e.PIECE_VALUE[QUEEN]);

	/* piece pairs */
	else if (!strncmp(line, "BISHOP_PAIR", 11))
		converted = sscanf(line, "BISHOP_PAIR %d", &e.BISHOP_PAIR);
	else if (!strncmp(line, "PENALTY_KNIGHT_PAIR", 19))
		converted = sscanf(line, "PENALTY_KNIGHT_PAIR %d", &e.P_KNIGHT_PAIR);
	else if (!strncmp(line, "PENALTY_ROOK_PAIR", 17))
		converted = sscanf(line, "PENALTY_ROOK_PAIR %d", &e.P_KNIGHT_PAIR);

	/* pawn shield */
	else if (!strncmp(line, "SHIELD_2", 8))
		converted = sscanf(line, "SHIELD_2 %d", &e.SHIELD_2);
	else if (!strncmp(line, "SHIELD_3", 8))
		converted = sscanf(line, "SHIELD_3 %d", &e.SHIELD_3);
	else if (!strncmp(line, "PENALTY_NO_SHIELD", 17))
		converted = sscanf(line, "PENALTY_NO_SHIELD %d", &e.P_NO_SHIELD);

	/* major penalties */
	else if (!strncmp(line, "PENALTY_BISHOP_TRAPPED_A7", 25))
		converted = sscanf(line, "PENALTY_BISHOP_TRAPPED_A7 %d", &e.P_BISHOP_TRAPPED_A7);
	else if (!strncmp(line, "PENALTY_BISHOP_TRAPPED_A6", 25))
		converted = sscanf(line, "PENALTY_BISHOP_TRAPPED_A6 %d", &e.P_BISHOP_TRAPPED_A6);
	else if (!strncmp(line, "PENALTY_KNIGHT_TRAPPED_A8", 25))
		converted = sscanf(line, "PENALTY_KNIGHT_TRAPPED_A8 %d", &e.P_KNIGHT_TRAPPED_A8);
	else if (!strncmp(line, "PENALTY_KNIGHT_TRAPPED_A7", 25))
		converted = sscanf(line, "PENALTY_KNIGHT_TRAPPED_A7 %d", &e.P_KNIGHT_TRAPPED_A7);
	else if (!strncmp(line, "PENALTY_KING_BLOCKS_ROOK", 24))
		converted = sscanf(line, "PENALTY_KNIGHT_TRAPPED_A7 %d", &e.P_KING_BLOCKS_ROOK);
	else if (!strncmp(line, "PENALTY_BLOCKED_CENTRAL_PAWN", 28))
		converted = sscanf(line, "PENALTY_BLOCKED_CENTRAL_PAWN %d", &e.P_BLOCK_CENTRAL_PAWN);

	/* minor penalties */
	else if (!strncmp(line, "PENALTY_KNIGHT_BLOCKS_C", 23))
		converted = sscanf(line, "PENALTY_KNIGHT_BLOCKS_C %d", &e.P_C3_KNIGHT);
	else if (!strncmp(line, "PENALTY_NO_FIANCHETTO", 21))
		converted = sscanf(line, "PENALTY_NO_FIANCHETTO %d", &e.P_NO_FIANCHETTO);

	/* minor positional bonuses */
	else if (!strncmp(line, "ROOK_OPEN", 9))
		converted = sscanf(line, "ROOK_OPEN %d", &e.ROOK_OPEN);
	else if (!strncmp(line, "ROOK_HALF_OPEN", 14))
		converted = sscanf(line, "ROOK_HALF_OPEN %d", &e.ROOK_HALF);
	else if (!strncmp(line, "FIANCHETTO", 10))
		converted = sscanf(line, "FIANCHETTO %d", &e.FIANCHETTO);
	else if (!strncmp(line, "RETURNING_BISHOP", 16))
		converted = sscanf(line, "RETURNING_BISHOP %d", &e.RETURNING_BISHOP);
	else if (!strncmp(line, "TEMPO", 5))
		converted = sscanf(line, "TEMPO %d", &e.TEMPO);

	/* variables deciding about inner workings of evaluation function */
	else if (!strncmp(line, "ENDGAME_MATERIAL", 16))
		converted = sscanf(line, "ENDGAME_MATERIAL %d", &e.ENDGAME_MAT);
}

int eval(int alpha, int beta, int use_hash) {
	int result = 0, mgScore = 0, egScore = 0;
	int stronger, weaker;

	/**************************************************************************
	*  Probe the evaluatinon hashtable, unless we call eval() only in order   *
	*  to display detailed result                                             *
	**************************************************************************/

	int probeval = tteval_probe();
	if (probeval != INVALID && use_hash)
		return probeval;

	/**************************************************************************
	*  Clear all eval data                                                    *
	**************************************************************************/

	v.gamePhase = board.piece_cnt[WHITE][KNIGHT] + board.piece_cnt[WHITE][BISHOP] + 2 * board.piece_cnt[WHITE][ROOK] + 4 * board.piece_cnt[WHITE][QUEEN]
		+ board.piece_cnt[BLACK][KNIGHT] + board.piece_cnt[BLACK][BISHOP] + 2 * board.piece_cnt[BLACK][ROOK] + 4 * board.piece_cnt[BLACK][QUEEN];

		for (int side = 0; side <= 1; side++) {
			v.mgMob[side] = 0;
			v.egMob[side] = 0;
			v.attCnt[side] = 0;
			v.attWeight[side] = 0;
			v.mgTropism[side] = 0;
			v.egTropism[side] = 0;
			v.adjustMaterial[side] = 0;
			v.blockages[side] = 0;
			v.positionalThemes[side] = 0;
			v.kingShield[side] = 0;
		}

		/**************************************************************************
		*  Sum the incrementally counted material and piece/square table values   *
		**************************************************************************/

		mgScore = board.piece_material[WHITE] + board.pawn_material[WHITE] + board.pcsq_mg[WHITE]
			- board.piece_material[BLACK] - board.pawn_material[BLACK] - board.pcsq_mg[BLACK];
			egScore = board.piece_material[WHITE] + board.pawn_material[WHITE] + board.pcsq_eg[WHITE]
				- board.piece_material[BLACK] - board.pawn_material[BLACK] - board.pcsq_eg[BLACK];

				/**************************************************************************
				* add king's pawn shield score and evaluate part of piece blockage score  *
				* (the rest of the latter will be done via piece eval)                    *
				**************************************************************************/

				v.kingShield[WHITE] = wKingShield();
				v.kingShield[BLACK] = bKingShield();
				blockedPieces(WHITE);
				blockedPieces(BLACK);
				mgScore += (v.kingShield[WHITE] - v.kingShield[BLACK]);

				/* tempo bonus */
				if (board.stm == WHITE) result += e.TEMPO;
				else				  result -= e.TEMPO;

				/**************************************************************************
				*  Adjusting material value for the various combinations of pieces.       *
				*  Currently it scores bishop, knight and rook pairs. The first one       *
				*  gets a bonus, the latter two - a penalty. Beside that knights lose     *
				*  value as pawns disappear, whereas rooks gain.                          *
				**************************************************************************/

				if (board.piece_cnt[WHITE][BISHOP] > 1) v.adjustMaterial[WHITE] += e.BISHOP_PAIR;
				if (board.piece_cnt[BLACK][BISHOP] > 1) v.adjustMaterial[BLACK] += e.BISHOP_PAIR;
				if (board.piece_cnt[WHITE][KNIGHT] > 1) v.adjustMaterial[WHITE] -= e.P_KNIGHT_PAIR;
				if (board.piece_cnt[BLACK][KNIGHT] > 1) v.adjustMaterial[BLACK] -= e.P_KNIGHT_PAIR;
				if (board.piece_cnt[WHITE][ROOK] > 1) v.adjustMaterial[WHITE] -= e.P_ROOK_PAIR;
				if (board.piece_cnt[BLACK][ROOK] > 1) v.adjustMaterial[BLACK] -= e.P_ROOK_PAIR;

				v.adjustMaterial[WHITE] += n_adj[board.piece_cnt[WHITE][PAWN]] * board.piece_cnt[WHITE][KNIGHT];
				v.adjustMaterial[BLACK] += n_adj[board.piece_cnt[BLACK][PAWN]] * board.piece_cnt[BLACK][KNIGHT];
				v.adjustMaterial[WHITE] += r_adj[board.piece_cnt[WHITE][PAWN]] * board.piece_cnt[WHITE][ROOK];
				v.adjustMaterial[BLACK] += r_adj[board.piece_cnt[BLACK][PAWN]] * board.piece_cnt[BLACK][ROOK];

				result += getPawnScore();

				/**************************************************************************
				*  Evaluate pieces                                                        *
				**************************************************************************/

				for (U8 row = 0; row < 8; row++)
					for (U8 col = 0; col < 8; col++) {

						S8 sq = SET_SQ(row, col);

						if (board.color[sq] != COLOR_EMPTY) {
							switch (board.pieces[sq]) {
							case PAWN: // pawns are evaluated separately
								break;
							case KNIGHT:
								EvalKnight(sq, board.color[sq]);
								break;
							case BISHOP:
								EvalBishop(sq, board.color[sq]);
								break;
							case ROOK:
								EvalRook(sq, board.color[sq]);
								break;
							case QUEEN:
								EvalQueen(sq, board.color[sq]);
								break;
							case KING:
								break;
							}
						}
					}

				/**************************************************************************
				*  Merge  midgame  and endgame score. We interpolate between  these  two  *
				*  values, using a gamePhase value, based on remaining piece material on  *
				*  both sides. With less pieces, endgame score becomes more influential.  *
				**************************************************************************/

				mgScore += (v.mgMob[WHITE] - v.mgMob[BLACK]);
				egScore += (v.egMob[WHITE] - v.egMob[BLACK]);
				mgScore += (v.mgTropism[WHITE] - v.mgTropism[BLACK]);
				egScore += (v.egTropism[WHITE] - v.egTropism[BLACK]);
				if (v.gamePhase > 24) v.gamePhase = 24;
				int mgWeight = v.gamePhase;
				int egWeight = 24 - mgWeight;
				result += ((mgScore * mgWeight) + (egScore * egWeight)) / 24;

				/**************************************************************************
				*  Add phase-independent score components.                                *
				**************************************************************************/

				result += (v.blockages[WHITE] - v.blockages[BLACK]);
				result += (v.positionalThemes[WHITE] - v.positionalThemes[BLACK]);
				result += (v.adjustMaterial[WHITE] - v.adjustMaterial[BLACK]);

				/**************************************************************************
				*  Merge king attack score. We don't apply this value if there are less   *
				*  than two attackers or if the attacker has no queen.                    *
				**************************************************************************/

				if (v.attCnt[WHITE] < 2 || board.piece_cnt[WHITE][QUEEN] == 0) v.attWeight[WHITE] = 0;
				if (v.attCnt[BLACK] < 2 || board.piece_cnt[BLACK][QUEEN] == 0) v.attWeight[BLACK] = 0;
				result += SafetyTable[v.attWeight[WHITE]];
				result -= SafetyTable[v.attWeight[BLACK]];

				/**************************************************************************
				*  Low material correction - guarding against an illusory material advan- *
				*  tage. Full blown program should have more such rules, but the current  *
				*  set ought to be useful enough. Please note that our code  assumes      *
				*  different material values for bishop and  knight.                      *
				*                                                                         *
				*  - a single minor piece cannot win                                      *
				*  - two knights cannot checkmate bare king                               *
				*  - bare rook vs minor piece is drawish                                  *
				*  - rook and minor vs rook is drawish                                    *
				**************************************************************************/

				if (result > 0) {
					stronger = WHITE;
					weaker = BLACK;
				}
				else {
					stronger = BLACK;
					weaker = WHITE;
				}

				if (board.pawn_material[stronger] == 0) {

					if (board.piece_material[stronger] < 400) return 0;

					if (board.pawn_material[weaker] == 0
						&& (board.piece_material[stronger] == 2 * e.PIECE_VALUE[KNIGHT]))
						return 0;

					if (board.piece_material[stronger] == e.PIECE_VALUE[ROOK]
						&& board.piece_material[weaker] == e.PIECE_VALUE[BISHOP]) result /= 2;

					if (board.piece_material[stronger] == e.PIECE_VALUE[ROOK]
						&& board.piece_material[weaker] == e.PIECE_VALUE[KNIGHT]) result /= 2;

					if (board.piece_material[stronger] == e.PIECE_VALUE[ROOK] + e.PIECE_VALUE[BISHOP]
						&& board.piece_material[weaker] == e.PIECE_VALUE[ROOK]) result /= 2;

					if (board.piece_material[stronger] == e.PIECE_VALUE[ROOK] + e.PIECE_VALUE[KNIGHT]
						&& board.piece_material[weaker] == e.PIECE_VALUE[ROOK]) result /= 2;
				}

				/**************************************************************************
				*  Finally return the score relative to the side to move.                 *
				**************************************************************************/

				if (board.stm == BLACK) result = -result;

				tteval_save(result);

				return result;
}

void EvalKnight(SQ sq, S8 side) {
	int att = 0;
	int mob = 0;
	int pos;

	/**************************************************************************
	*  Collect data about mobility and king attacks. This resembles move      *
	*  generation code, except that we are just incrementing the counters     *
	*  instead of adding actual moves.                                        *
	**************************************************************************/

	for (U8 dir = 0; dir < 8; dir++) {
		pos = sq + vector[KNIGHT][dir];
		if (IS_SQ(pos) && board.color[pos] != side) {
			// we exclude mobility to squares controlled by enemy pawns
			// but don't penalize possible captures
			if (!board.pawn_ctrl[!side][pos]) ++mob;
			if (e.sqNearK[!side][board.king_loc[!side]][pos])
				++att; // this knight is attacking zone around enemy king
		}
	}

	/**************************************************************************
	*  Evaluate mobility. We try to do it in such a way that zero represents  *
	*  average mobility, but  our formula of doing so is a puer guess.        *
	**************************************************************************/

	v.mgMob[side] += 4 * (mob - 4);
	v.egMob[side] += 4 * (mob - 4);

	/**************************************************************************
	*  Save data about king attacks                                           *
	**************************************************************************/

	if (att) {
		v.attCnt[side]++;
		v.attWeight[side] += 2 * att;
	}

	/**************************************************************************
	* Evaluate king tropism                                                   *
	**************************************************************************/

	int tropism = getTropism(sq, board.king_loc[!side]);
	v.mgTropism[side] += 3 * tropism;
	v.egTropism[side] += 3 * tropism;
}

void EvalBishop(SQ sq, S8 side) {

	int att = 0;
	int mob = 0;

	/**************************************************************************
	*  Collect data about mobility and king attacks                           *
	**************************************************************************/

	for (char dir = 0; dir < vectors[BISHOP]; dir++) {

		for (char pos = sq;;) {

			pos = pos + vector[BISHOP][dir];
			if (!IS_SQ(pos)) break;

			if (board.pieces[pos] == PIECE_EMPTY) {
				if (!board.pawn_ctrl[!side][pos]) mob++;
				// we exclude mobility to squares controlled by enemy pawns
				if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
			}
			else {                                // non-empty square
				if (board.color[pos] != side) {         // opponent's piece
					mob++;
					if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
				}
				break;                              // own piece
			}
		}
	}

	v.mgMob[side] += 3 * (mob - 7);
	v.egMob[side] += 3 * (mob - 7);

	if (att) {
		v.attCnt[side]++;
		v.attWeight[side] += 2 * att;
	}

	int tropism = getTropism(sq, board.king_loc[!side]);
	v.mgTropism[side] += 2 * tropism;
	v.egTropism[side] += 1 * tropism;
}

void EvalRook(SQ sq, S8 side) {

	int att = 0;
	int mob = 0;

	/**************************************************************************
	*  Bonus for rook on the seventh rank. It is applied when there are pawns *
	*  to attack along that rank or if enemy king is cut off on 8th rank      *
	/*************************************************************************/

	if (ROW(sq) == seventh[side]
		&& (board.pawns_on_rank[!side][seventh[side]] || ROW(board.king_loc[!side]) == eighth[side])) {
		v.mgMob[side] += 20;
		v.egMob[side] += 30;
	}

	/**************************************************************************
	*  Bonus for open and half-open files is merged with mobility score.      *
	*  Bonus for open files targetting enemy king is added to attWeight[]     *
	/*************************************************************************/

	if (board.pawns_on_file[side][COL(sq)] == 0) {
		if (board.pawns_on_file[!side][COL(sq)] == 0) { // fully open file
			v.mgMob[side] += e.ROOK_OPEN;
			v.egMob[side] += e.ROOK_OPEN;
			if (abs(COL(sq) - COL(board.king_loc[!side])) < 2)
				v.attWeight[side] += 1;
		}
		else {                                    // half open file
			v.mgMob[side] += e.ROOK_HALF;
			v.egMob[side] += e.ROOK_HALF;
			if (abs(COL(sq) - COL(board.king_loc[!side])) < 2)
				v.attWeight[side] += 2;
		}
	}

	/**************************************************************************
	*  Collect data about mobility and king attacks                           *
	**************************************************************************/

	for (char dir = 0; dir < vectors[ROOK]; dir++) {

		for (char pos = sq;;) {

			pos = pos + vector[ROOK][dir];
			if (!IS_SQ(pos)) break;

			if (board.pieces[pos] == PIECE_EMPTY) {
				mob++;
				if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
			}
			else {                                // non-empty square
				if (board.color[pos] != side) {         // opponent's piece
					mob++;
					if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
				}
				break;                              // own piece
			}
		}
	}

	v.mgMob[side] += 2 * (mob - 7);
	v.egMob[side] += 4 * (mob - 7);

	if (att) {
		v.attCnt[side]++;
		v.attWeight[side] += 3 * att;
	}

	int tropism = getTropism(sq, board.king_loc[!side]);
	v.mgTropism[side] += 2 * tropism;
	v.egTropism[side] += 1 * tropism;
}

void EvalQueen(SQ sq, S8 side) {

	int att = 0;
	int mob = 0;

	if (ROW(sq) == seventh[side]
		&& (board.pawns_on_rank[!side][seventh[side]] || ROW(board.king_loc[!side]) == eighth[side])) {
		v.mgMob[side] += 5;
		v.egMob[side] += 10;
	}

	/**************************************************************************
	*  A queen should not be developed too early                              *
	**************************************************************************/

	if ((side == WHITE && ROW(sq) > ROW_2) || (side == BLACK && ROW(sq) < ROW_7)) {
		if (isPiece(side, KNIGHT, REL_SQ(side, B1))) v.positionalThemes[side] -= 2;
		if (isPiece(side, BISHOP, REL_SQ(side, C1))) v.positionalThemes[side] -= 2;
		if (isPiece(side, BISHOP, REL_SQ(side, F1))) v.positionalThemes[side] -= 2;
		if (isPiece(side, KNIGHT, REL_SQ(side, G1))) v.positionalThemes[side] -= 2;
	}

	/**************************************************************************
	*  Collect data about mobility and king attacks                           *
	**************************************************************************/

	for (char dir = 0; dir < vectors[QUEEN]; dir++) {

		for (char pos = sq;;) {

			pos = pos + vector[QUEEN][dir];
			if (!IS_SQ(pos)) break;

			if (board.pieces[pos] == PIECE_EMPTY) {
				mob++;
				if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
			}
			else {                                 // non-empty square
				if (board.color[pos] != side) {          // opponent's piece
					mob++;
					if (e.sqNearK[!side][board.king_loc[!side]][pos]) ++att;
				}
				break;                               // own piece
			}
		}
	}

	v.mgMob[side] += 1 * (mob - 14);
	v.egMob[side] += 2 * (mob - 14);

	if (att) {
		v.attCnt[side]++;
		v.attWeight[side] += 4 * att;
	}

	int tropism = getTropism(sq, board.king_loc[!side]);
	v.mgTropism[side] += 2 * tropism;
	v.egTropism[side] += 4 * tropism;
}

int wKingShield() {

	int result = 0;

	/* king on the kingside */
	if (COL(board.king_loc[WHITE]) > COL_E) {

		if (isPiece(WHITE, PAWN, F2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, F3))  result += e.SHIELD_3;

		if (isPiece(WHITE, PAWN, G2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, G3))  result += e.SHIELD_3;

		if (isPiece(WHITE, PAWN, H2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, H3))  result += e.SHIELD_3;
	}

	/* king on the queenside */
	else if (COL(board.king_loc[WHITE]) < COL_D) {

		if (isPiece(WHITE, PAWN, A2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, A3))  result += e.SHIELD_3;

		if (isPiece(WHITE, PAWN, B2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, B3))  result += e.SHIELD_3;

		if (isPiece(WHITE, PAWN, C2))  result += e.SHIELD_2;
		else if (isPiece(WHITE, PAWN, C3))  result += e.SHIELD_3;
	}

	return result;
}

int bKingShield() {
	int result = 0;

	/* king on the kingside */
	if (COL(board.king_loc[BLACK]) > COL_E) {
		if (isPiece(BLACK, PAWN, F7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, F6))  result += e.SHIELD_3;

		if (isPiece(BLACK, PAWN, G7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, G6))  result += e.SHIELD_3;

		if (isPiece(BLACK, PAWN, H7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, H6))  result += e.SHIELD_3;
	}

	/* king on the queenside */
	else if (COL(board.king_loc[BLACK]) < COL_D) {
		if (isPiece(BLACK, PAWN, A7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, A6))  result += e.SHIELD_3;

		if (isPiece(BLACK, PAWN, B7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, B6))  result += e.SHIELD_3;

		if (isPiece(BLACK, PAWN, C7))  result += e.SHIELD_2;
		else if (isPiece(BLACK, PAWN, C6))  result += e.SHIELD_3;
	}
	return result;
}

/******************************************************************************
*                            Pawn structure evaluaton                         *
******************************************************************************/

int getPawnScore() {
	int result;

	/**************************************************************************
	*  This function wraps hashing mechanism around evalPawnStructure().      *
	*  Please note  that since we use the pawn hashtable, evalPawnStructure() *
	*  must not take into account the piece position.  In a more elaborate    *
	*  program, pawn hashtable would contain only the characteristics of pawn *
	*  structure,  and scoring them in conjunction with the piece position    *
	*  would have been done elsewhere.                                        *
	**************************************************************************/

	int probeval = ttpawn_probe();
	if (probeval != INVALID)
		return probeval;

	result = evalPawnStructure();
	ttpawn_save(result);
	return result;
}

int evalPawnStructure() {
	int result = 0;

	for (U8 row = 0; row < 8; row++)
		for (U8 col = 0; col < 8; col++) {

			S8 sq = SET_SQ(row, col);

			if (board.pieces[sq] == PAWN) {
				if (board.color[sq] == WHITE) result += EvalPawn(sq, WHITE);
				else                      result -= EvalPawn(sq, BLACK);
			}
		}

	return result;
}

int EvalPawn(SQ sq, S8 side) {
	int result = 0;
	int flagIsPassed = 1; // we will be trying to disprove that
	int flagIsWeak = 1;   // we will be trying to disprove that
	int flagIsOpposed = 0;

	/**************************************************************************
	*   We have only very basic data structures that do not update informa-   *
	*   tion about pawns incrementally, so we have to calculate everything    *
	*   here.  The loop below detects doubled pawns, passed pawns and sets    *
	*   a flag on finding that our pawn is opposed by enemy pawn.             *
	**************************************************************************/

	if (board.pawn_ctrl[!side][sq]) // if a pawn is attacked by a pawn, it is not
		flagIsPassed = 0;       // passed (not sure if it's the best decision)

	S8 nextSq = sq + stepFwd[side];

	while (IS_SQ(nextSq)) {

		if (board.pieces[nextSq] == PAWN) { // either opposed by enemy pawn or doubled
			flagIsPassed = 0;
			if (board.color[nextSq] == side)
				result -= 20;       // doubled pawn penalty
			else
				flagIsOpposed = 1;  // flag our pawn as opposed
		}

		if (board.pawn_ctrl[!side][nextSq])
			flagIsPassed = 0;

		nextSq += stepFwd[side];
	}

	/**************************************************************************
	*   Another loop, going backwards and checking whether pawn has support.  *
	*   Here we can at least break out of it for speed optimization.          *
	**************************************************************************/

	nextSq = sq + stepFwd[side]; // so that a pawn in a duo will not be considered weak

	while (IS_SQ(nextSq)) {

		if (board.pawn_ctrl[side][nextSq]) {
			flagIsWeak = 0;
			break;
		}

		nextSq += stepBck[side];
	}

	/**************************************************************************
	*  Evaluate passed pawns, scoring them higher if they are protected       *
	*  or if their advance is supported by friendly pawns                     *
	**************************************************************************/

	if (flagIsPassed) {
		if (isPawnSupported(sq, side)) result += e.protected_passer[side][sq];
		else							 result += e.passed_pawn[side][sq];
	}

	/**************************************************************************
	*  Evaluate weak pawns, increasing the penalty if they are situated       *
	*  on a half-open file                                                    *
	**************************************************************************/

	if (flagIsWeak) {
		result += e.weak_pawn[side][sq];
		if (!flagIsOpposed)
			result -= 4;
	}

	return result;
}

bool isPawnSupported(SQ sq, S8 side)
{
	int step = side == WHITE ? SOUTH : NORTH;
	if (IS_SQ(sq + WEST) && isPiece(side, PAWN, sq + WEST)) return 1;
	if (IS_SQ(sq + EAST) && isPiece(side, PAWN, sq + EAST)) return 1;
	if (IS_SQ(sq + step + WEST) && isPiece(side, PAWN, sq + step + WEST)) return 1;
	if (IS_SQ(sq + step + EAST) && isPiece(side, PAWN, sq + step + EAST)) return 1;
	return 0;
}

/******************************************************************************
*                             Pattern detection                               *
******************************************************************************/
void blockedPieces(int side)
{
	unsigned char oppo = side ^ 1;
	// central pawn blocked, bishop hard to develop
	if (isPiece(side, BISHOP, REL_SQ(side, C1))
		&& isPiece(side, PAWN, REL_SQ(side, D2))
		&& board.color[REL_SQ(side, D3)] != COLOR_EMPTY)
		v.blockages[side] -= e.P_BLOCK_CENTRAL_PAWN;

	if (isPiece(side, BISHOP, REL_SQ(side, F1))
		&& isPiece(side, PAWN, REL_SQ(side, E2))
		&& board.color[REL_SQ(side, E3)] != COLOR_EMPTY)
		v.blockages[side] -= e.P_BLOCK_CENTRAL_PAWN;

	// trapped knight
	if (isPiece(side, KNIGHT, REL_SQ(side, A8))
		&& (isPiece(oppo, PAWN, REL_SQ(side, A7)) || isPiece(oppo, PAWN, REL_SQ(side, C7))))
		v.blockages[side] -= e.P_KNIGHT_TRAPPED_A8;

	if (isPiece(side, KNIGHT, REL_SQ(side, H8))
		&& (isPiece(oppo, PAWN, REL_SQ(side, H7)) || isPiece(oppo, PAWN, REL_SQ(side, F7))))
		v.blockages[side] -= e.P_KNIGHT_TRAPPED_A8;

	if (isPiece(side, KNIGHT, REL_SQ(side, A7))
		&& isPiece(oppo, PAWN, REL_SQ(side, A6))
		&& isPiece(oppo, PAWN, REL_SQ(side, B7)))
		v.blockages[side] -= e.P_KNIGHT_TRAPPED_A7;

	if (isPiece(side, KNIGHT, REL_SQ(side, H7))
		&& isPiece(oppo, PAWN, REL_SQ(side, H6))
		&& isPiece(oppo, PAWN, REL_SQ(side, G7)))
		v.blockages[side] -= e.P_KNIGHT_TRAPPED_A7;

	// knight blocking queenside pawns
	if (isPiece(side, KNIGHT, REL_SQ(side, C3))
		&& isPiece(side, PAWN, REL_SQ(side, C2))
		&& isPiece(side, PAWN, REL_SQ(side, D4))
		&& !isPiece(side, PAWN, REL_SQ(side, E4)))
		v.blockages[side] -= e.P_C3_KNIGHT;

	// trapped bishop
	if (isPiece(side, BISHOP, REL_SQ(side, A7))
		&& isPiece(oppo, PAWN, REL_SQ(side, B6)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A7;

	if (isPiece(side, BISHOP, REL_SQ(side, H7))
		&& isPiece(oppo, PAWN, REL_SQ(side, G6)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A7;

	if (isPiece(side, BISHOP, REL_SQ(side, B8))
		&& isPiece(oppo, PAWN, REL_SQ(side, C7)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A7;

	if (isPiece(side, BISHOP, REL_SQ(side, G8))
		&& isPiece(oppo, PAWN, REL_SQ(side, F7)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A7;

	if (isPiece(side, BISHOP, REL_SQ(side, A6))
		&& isPiece(oppo, PAWN, REL_SQ(side, B5)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A6;

	if (isPiece(side, BISHOP, REL_SQ(side, H6))
		&& isPiece(oppo, PAWN, REL_SQ(side, G5)))
		v.blockages[side] -= e.P_BISHOP_TRAPPED_A6;

	// bishop on initial sqare supporting castled king
	if (isPiece(side, BISHOP, REL_SQ(side, F1))
		&& isPiece(side, KING, REL_SQ(side, G1)))
		v.positionalThemes[side] += e.RETURNING_BISHOP;

	if (isPiece(side, BISHOP, REL_SQ(side, C1))
		&& isPiece(side, KING, REL_SQ(side, B1)))
		v.positionalThemes[side] += e.RETURNING_BISHOP;

	// uncastled king blocking own rook
	if ((isPiece(side, KING, REL_SQ(side, F1)) || isPiece(side, KING, REL_SQ(side, G1)))
		&& (isPiece(side, ROOK, REL_SQ(side, H1)) || isPiece(side, ROOK, REL_SQ(side, G1))))
		v.blockages[side] -= e.P_KING_BLOCKS_ROOK;

	if ((isPiece(side, KING, REL_SQ(side, C1)) || isPiece(side, KING, REL_SQ(side, B1)))
		&& (isPiece(side, ROOK, REL_SQ(side, A1)) || isPiece(side, ROOK, REL_SQ(side, B1))))
		v.blockages[side] -= e.P_KING_BLOCKS_ROOK;
}

int isPiece(U8 color, U8 piece, SQ sq)
{
	return ((board.pieces[sq] == piece) && (board.color[sq] == color));
}

void PrintBoard() {
	int r, f, sq;
	string uw = "PNBRQKXX";
	string ub = "pnbrqkxx";
	cout << "    a b c d e f g h" << endl;
	cout << "   ----------------" << endl;
	for (r = 7; r >= 0; r--) {
		printf(" %d|", r + 1);
		for (f = 0; f <= 7; f++) {
			sq = SET_SQ(r, f);
			int piece = board.pieces[sq];
			if (piece == PIECE_EMPTY)
				printf(" .");
			else if (board.color[sq] == WHITE)
				printf(" %c", uw[piece & 0x7]);
			else if (board.color[sq] == BLACK)
				printf(" %c", ub[piece & 0x7]);
		}
		cout << endl;
	}
}

/******************************************************************************
*                             Printing eval results                           *
******************************************************************************/
void printEval() {
	PrintBoard();
	printf("------------------------------------------\n");
	printf("Total value (for side to move): %d \n", eval(-INF, INF, 0));
	printf("Material balance       : %d \n", board.piece_material[WHITE] + board.pawn_material[WHITE] - board.piece_material[BLACK] - board.pawn_material[BLACK]);
	printf("Material adjustement   : ");
	printEvalFactor(v.adjustMaterial[WHITE], v.adjustMaterial[BLACK]);
	printf("Mg Piece/square tables : ");
	printEvalFactor(board.pcsq_mg[WHITE], board.pcsq_mg[BLACK]);
	printf("Eg Piece/square tables : ");
	printEvalFactor(board.pcsq_eg[WHITE], board.pcsq_eg[BLACK]);
	printf("Mg Mobility            : ");
	printEvalFactor(v.mgMob[WHITE], v.mgMob[BLACK]);
	printf("Eg Mobility            : ");
	printEvalFactor(v.egMob[WHITE], v.egMob[BLACK]);
	printf("Mg Tropism             : ");
	printEvalFactor(v.mgTropism[WHITE], v.mgTropism[BLACK]);
	printf("Eg Tropism             : ");
	printEvalFactor(v.egTropism[WHITE], v.egTropism[BLACK]);
	printf("Pawn structure         : %d \n", evalPawnStructure());
	printf("Blockages              : ");
	printEvalFactor(v.blockages[WHITE], v.blockages[BLACK]);
	printf("Positional themes      : ");
	printEvalFactor(v.positionalThemes[WHITE], v.positionalThemes[BLACK]);
	printf("King Shield            : ");
	printEvalFactor(v.kingShield[WHITE], v.kingShield[BLACK]);
	printf("Tempo                  : ");
	if (board.stm == WHITE) printf("%d", e.TEMPO);
	else printf("%d", -e.TEMPO);
	printf("\n");
	printf("------------------------------------------\n");
}

void printEvalFactor(int wh, int bl)
{
	printf("white %4d, black %4d, total: %4d \n", wh, bl, wh - bl);
}

int getTropism(int sq1, int sq2)
{
	return 7 - (abs(ROW(sq1) - ROW(sq2)) + abs(COL(sq1) - COL(sq2)));
}
