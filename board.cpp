#include "board.h"

sboard board = {};

void clearBoard()
{
	for (int sq = 0; sq < 128; sq++)
	{
		board.pieces[sq] = PIECE_EMPTY;
		board.color[sq] = COLOR_EMPTY;
		for (int cl = 0; cl < 2; cl++) {
			board.pawn_ctrl[cl][sq] = 0;
		}
	}
	board.castle = 0;
	board.ep = -1;
	board.ply = 0;
	board.hash = 0;
	board.phash = 0;
	board.stm = 0;
	board.rep_index = 0;

	// reset perceived values

	board.piece_material[WHITE] = 0;
	board.piece_material[BLACK] = 0;
	board.pawn_material[WHITE] = 0;
	board.pawn_material[BLACK] = 0;
	board.pcsq_mg[WHITE] = 0;
	board.pcsq_mg[BLACK] = 0;
	board.pcsq_eg[WHITE] = 0;
	board.pcsq_eg[BLACK] = 0;


	// reset counters

	for (int i = 0; i < 6; i++) {
		board.piece_cnt[WHITE][i] = 0;
		board.piece_cnt[BLACK][i] = 0;
	}

	for (int i = 0; i < 8; i++) {
		board.pawns_on_file[WHITE][i] = 0;
		board.pawns_on_file[BLACK][i] = 0;
		board.pawns_on_rank[WHITE][i] = 0;
		board.pawns_on_rank[BLACK][i] = 0;
	}
}

/******************************************************************************
* fillSq() and clearSq(), beside placing a piece on a given square or erasing *
* it,  must  take care for all the incrementally updated  stuff:  hash  keys, *
* piece counters, material and pcsq values, pawn-related data, king location. *
******************************************************************************/

void FillSq(U8 color, U8 piece, S8 sq) {

	// place a piece on the board
	board.pieces[sq] = piece;
	board.color[sq] = color;

	// update king location
	if (piece == KING)
		board.king_loc[color] = sq;

	/**************************************************************************
	* Pawn structure changes slower than piece position, which allows reusing *
	* some data, both in pawn and piece evaluation. For that reason we do     *
	* some extra work here, expecting to gain extra speed elsewhere.          *
	**************************************************************************/

	if (piece == PAWN) {
		// update pawn material
		board.pawn_material[color] += e.PIECE_VALUE[piece];

		// update pawn hashkey
		board.phash ^= zobrist.piecesquare[piece][color][sq];

		// update counter of pawns on a given rank and file
		++board.pawns_on_file[color][COL(sq)];
		++board.pawns_on_rank[color][ROW(sq)];

		// update squares controlled by pawns
		if (color == WHITE) {
			if (IS_SQ(sq + NE)) board.pawn_ctrl[WHITE][sq + NE]++;
			if (IS_SQ(sq + NW)) board.pawn_ctrl[WHITE][sq + NW]++;
		}
		else {
			if (IS_SQ(sq + SE)) board.pawn_ctrl[BLACK][sq + SE]++;
			if (IS_SQ(sq + SW)) board.pawn_ctrl[BLACK][sq + SW]++;
		}
	}
	else {
		// update piece material
		board.piece_material[color] += e.PIECE_VALUE[piece];
	}

	// update piece counter
	board.piece_cnt[color][piece]++;

	// update piece-square value
	board.pcsq_mg[color] += e.mgPst[piece][color][sq];
	board.pcsq_eg[color] += e.egPst[piece][color][sq];

	// update hash key
	board.hash ^= zobrist.piecesquare[piece][color][sq];
}


void ClearSq(SQ sq) {

	// set intermediate variables, then do the same
	// as in fillSq(), only backwards

	U8 color = board.color[sq];
	U8 piece = board.pieces[sq];

	board.hash ^= zobrist.piecesquare[piece][color][sq];

	if (piece == PAWN) {
		// update squares controlled by pawns
		if (color == WHITE) {
			if (IS_SQ(sq + NE)) board.pawn_ctrl[WHITE][sq + NE]--;
			if (IS_SQ(sq + NW)) board.pawn_ctrl[WHITE][sq + NW]--;
		}
		else {
			if (IS_SQ(sq + SE)) board.pawn_ctrl[BLACK][sq + SE]--;
			if (IS_SQ(sq + SW)) board.pawn_ctrl[BLACK][sq + SW]--;
		}

		--board.pawns_on_file[color][COL(sq)];
		--board.pawns_on_rank[color][ROW(sq)];
		board.pawn_material[color] -= e.PIECE_VALUE[piece];
		board.phash ^= zobrist.piecesquare[piece][color][sq];
	}
	else
		board.piece_material[color] -= e.PIECE_VALUE[piece];

	board.pcsq_mg[color] -= e.mgPst[piece][color][sq];
	board.pcsq_eg[color] -= e.egPst[piece][color][sq];

	board.piece_cnt[color][piece]--;

	board.pieces[sq] = PIECE_EMPTY;
	board.color[sq] = COLOR_EMPTY;
}


int board_loadFromFen(char* fen) {

	clearBoard();
	clearHistoryTable();

	char* f = fen;

	char col = 0;
	char row = 7;

	do {
		switch (f[0]) {
		case 'K':
			FillSq(WHITE, KING, SET_SQ(row, col));
			col++;
			break;
		case 'Q':
			FillSq(WHITE, QUEEN, SET_SQ(row, col));
			col++;
			break;
		case 'R':
			FillSq(WHITE, ROOK, SET_SQ(row, col));
			col++;
			break;
		case 'B':
			FillSq(WHITE, BISHOP, SET_SQ(row, col));
			col++;
			break;
		case 'N':
			FillSq(WHITE, KNIGHT, SET_SQ(row, col));
			col++;
			break;
		case 'P':
			FillSq(WHITE, PAWN, SET_SQ(row, col));
			col++;
			break;
		case 'k':
			FillSq(BLACK, KING, SET_SQ(row, col));
			col++;
			break;
		case 'q':
			FillSq(BLACK, QUEEN, SET_SQ(row, col));
			col++;
			break;
		case 'r':
			FillSq(BLACK, ROOK, SET_SQ(row, col));
			col++;
			break;
		case 'b':
			FillSq(BLACK, BISHOP, SET_SQ(row, col));
			col++;
			break;
		case 'n':
			FillSq(BLACK, KNIGHT, SET_SQ(row, col));
			col++;
			break;
		case 'p':
			FillSq(BLACK, PAWN, SET_SQ(row, col));
			col++;
			break;
		case '/':
			row--;
			col = 0;
			break;
		case '1':
			col += 1;
			break;
		case '2':
			col += 2;
			break;
		case '3':
			col += 3;
			break;
		case '4':
			col += 4;
			break;
		case '5':
			col += 5;
			break;
		case '6':
			col += 6;
			break;
		case '7':
			col += 7;
			break;
		case '8':
			col += 8;
			break;
		};

		f++;
	} while (f[0] != ' ');

	f++;

	if (f[0] == 'w') {
		board.stm = WHITE;
	}
	else {
		board.stm = BLACK;
		board.hash ^= zobrist.color;
	}

	f += 2;

	do {
		switch (f[0]) {
		case 'K':
			board.castle |= CASTLE_WK;
			break;
		case 'Q':
			board.castle |= CASTLE_WQ;
			break;
		case 'k':
			board.castle |= CASTLE_BK;
			break;
		case 'q':
			board.castle |= CASTLE_BQ;
			break;
		}

		f++;
	} while (f[0] != ' ');

	board.hash ^= zobrist.castling[board.castle];

	f++;

	if (f[0] != '-') {
		board.ep = convert_a_0x88(f);
		board.hash ^= zobrist.ep[board.ep];
	}

	do {
		f++;
	} while (f[0] != ' ');
	f++;
	int ply = 0;
	int converted;
	converted = sscanf(f, "%d", &ply);
	board.ply = (unsigned char)ply;

	board.rep_index = 0;
	board.rep_stack[board.rep_index] = board.hash;

	return 0;
}


void board_display() {

	S8 sq;

	char parray[3][7] = { {'K','Q','R','B','N','P'},
		{'k','q','r','b','n','p'},
		{ 0 , 0 , 0 , 0 , 0,  0, '.'}
	};

	printf("   a b c d e f g h\n\n");

	for (S8 row = 7; row >= 0; row--) {

		printf("%d ", row + 1);

		for (S8 col = 0; col < 8; col++) {
			sq = SET_SQ(row, col);
			printf(" %c", parray[board.color[sq]][board.pieces[sq]]);
		}

		printf("  %d\n", row + 1);

	}

	printf("\n   a b c d e f g h\n\n");
}