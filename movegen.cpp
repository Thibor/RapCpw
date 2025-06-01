#include "program.h"

U8 movecount;

smove* m;

bool slide[5] = { 0, 1, 1, 1, 0 };
char vectors[5] = { 8, 8, 4, 4, 8 };
char vector[5][8] = {
	{ SW, SOUTH, SE, WEST, EAST, NW, NORTH, NE },
	{ SW, SOUTH, SE, WEST, EAST, NW, NORTH, NE },
	{ SOUTH, WEST, EAST, NORTH                 },
	{ SW, SE, NW, NE                           },
	{ -33, -31, -18, -14, 14, 18, 31, 33       }
};

int move_makeNull() {
	board.stm ^= 1;
	board.hash ^= zobrist.color;
	board.ply++;
	if (board.ep != -1) {
		board.hash ^= zobrist.ep[board.ep];
		board.ep = -1;
	}
	return 0;
}

int move_unmakeNull(char ep) {
	board.stm ^= 1;
	board.hash ^= zobrist.color;
	board.ply--;
	if (ep != -1) {
		board.hash ^= zobrist.ep[ep];
		board.ep = ep;
	}
	return 0;
}

int move_make(smove move) {

	/* switch the side to move */
	board.stm ^= 1;
	board.hash ^= zobrist.color;

	/* a capture or a pawn move clears b.ply */
	board.ply++;
	if ((move.piece_from == PAWN) || move_iscapt(move))
		board.ply = 0;

	/* in case of a capture, the "to" square must be cleared,
	   else incrementally updated stuff gets blown up */
	if (board.pieces[move.to] != PIECE_EMPTY)
		ClearSq(move.to);

	/* a piece vacates its initial square */
	ClearSq(move.from);

	/* a piece arrives to its destination square */
	FillSq(board.stm ^ 1, move.piece_to, move.to);

	/**************************************************************************
	*  Reset the castle flags. If either a king or a rook leaves its initial  *
	*  square, the side looses the castling rights. The same happens when     *
	*  a rook on its initial square gets captured.                            *
	**************************************************************************/

	switch (move.from) {
	case H1:
		board.castle &= ~CASTLE_WK;
		break;
	case E1:
		board.castle &= ~(CASTLE_WK | CASTLE_WQ);
		break;
	case A1:
		board.castle &= ~CASTLE_WQ;
		break;
	case H8:
		board.castle &= ~CASTLE_BK;
		break;
	case E8:
		board.castle &= ~(CASTLE_BK | CASTLE_BQ);
		break;
	case A8:
		board.castle &= ~CASTLE_BQ;
		break;
	}
	switch (move.to) {
	case H1:
		board.castle &= ~CASTLE_WK;
		break;
	case E1:
		board.castle &= ~(CASTLE_WK | CASTLE_WQ);
		break;
	case A1:
		board.castle &= ~CASTLE_WQ;
		break;
	case H8:
		board.castle &= ~CASTLE_BK;
		break;
	case E8:
		board.castle &= ~(CASTLE_BK | CASTLE_BQ);
		break;
	case A8:
		board.castle &= ~CASTLE_BQ;
		break;
	}
	board.hash ^= zobrist.castling[move.castle];
	board.hash ^= zobrist.castling[board.castle];

	/**************************************************************************
	*   Finish the castling move. It is represented as the king move (e1g1    *
	*   = White castles short), which has already been executed above. Now    *
	*   we must move the rook to complete castling.                           *
	**************************************************************************/

	if (move.flags & MFLAG_CASTLE) {
		if (move.to == G1) {
			ClearSq(H1);
			FillSq(WHITE, ROOK, F1);
		}
		else if (move.to == C1) {
			ClearSq(A1);
			FillSq(WHITE, ROOK, D1);
		}
		else if (move.to == G8) {
			ClearSq(H8);
			FillSq(BLACK, ROOK, F8);
		}
		else if (move.to == C8) {
			ClearSq(A8);
			FillSq(BLACK, ROOK, D8);
		}
	}

	/**************************************************************************
	*  Erase the current state of the ep-flag, then set it again if a pawn    *
	*  jump that allows such capture has been made. 1.e4 in the initial po-   *
	*  sition will not set the en passant flag, because there are no black    *
	*  pawns on d4 and f4. This soluion helps with opening book and increa-   *
	*  ses the number of transposition table hits.                            *
	**************************************************************************/

	if (board.ep != -1) {
		board.hash ^= zobrist.ep[board.ep];
		board.ep = -1;
	}
	if ((move.piece_from == PAWN) && (abs(move.from - move.to) == 32)
		&& (board.pawn_ctrl[board.stm][(move.from + move.to) / 2])
		) {
		board.ep = (move.from + move.to) / 2;
		board.hash ^= zobrist.ep[board.ep];
	}

	/**************************************************************************
	*  Remove a pawn captured en passant                                      *
	**************************************************************************/

	if (move.flags & MFLAG_EPCAPTURE) {
		if (!board.stm == WHITE) {
			ClearSq(move.to - 16);
		}
		else {
			ClearSq(move.to + 16);
		}
	}

	++board.rep_index;
	board.rep_stack[board.rep_index] = board.hash;

	return 0;
}

int move_unmake(smove move) {

	board.stm ^= 1;
	board.hash ^= zobrist.color;

	board.ply = move.ply;

	/* set en passant square */
	if (board.ep != -1)
		board.hash ^= zobrist.ep[board.ep];
	if (move.ep != -1)
		board.hash ^= zobrist.ep[move.ep];
	board.ep = move.ep;

	/* Move the piece back */
	ClearSq(move.to);
	FillSq(board.stm, move.piece_from, move.from);

	/* Un-capture: in case of a capture, put the captured piece back */
	if (move_iscapt(move))
		FillSq((char)!board.stm, move.piece_cap, move.to);

	/* Un-castle: the king has already been moved, now move the rook */
	if (move.flags & MFLAG_CASTLE) {
		if (move.to == G1) {
			ClearSq(F1);
			FillSq(WHITE, ROOK, H1);
		}
		else if (move.to == C1) {
			ClearSq(D1);
			FillSq(WHITE, ROOK, A1);
		}
		else if (move.to == G8) {
			ClearSq(F8);
			FillSq(BLACK, ROOK, H8);
		}
		else if (move.to == C8) {
			ClearSq(D8);
			FillSq(BLACK, ROOK, A8);
		}
	}

	/* adjust castling flags */
	board.hash ^= zobrist.castling[move.castle];
	board.hash ^= zobrist.castling[board.castle];
	board.castle = move.castle;

	/* Put the pawn captured en passant back to its initial square */
	if (move.flags & MFLAG_EPCAPTURE) {
		if (board.stm == WHITE) {
			FillSq(BLACK, PAWN, move.to - 16);
		}
		else {
			FillSq(WHITE, PAWN, move.to + 16);
		}
	}

	--board.rep_index;

	return 0;
}

int move_iscapt(smove m) {
	return (m.piece_cap != PIECE_EMPTY);
}

int move_isprom(smove m) {
	return (m.piece_from != m.piece_to);
}

int move_canSimplify(smove m) {
	if (m.piece_cap == PAWN
		|| board.piece_material[!board.stm] - e.PIECE_VALUE[m.piece_cap] > e.ENDGAME_MAT)
		return 0;
	else
		return 1;
}

int move_countLegal() {
	smove mlist[256];
	int mcount = movegen(mlist, 0xFF);
	int result = 0;

	for (int i = 0; i < mcount; i++) {

		/* try a move... */
		move_make(mlist[i]);

		/* ...then increase the counter if it did not leave us in check */
		if (!isAttacked(board.stm, board.king_loc[!board.stm])) ++result;

		move_unmake(mlist[i]);
	}

	/* return number of legal moves in the current position */
	return result;
}

bool move_isLegal(smove m)
{
	smove movelist[256] = {};
	int movecount = movegen(movelist, 0xFF);
	for (int i = 0; i < movecount; i++)
		if (movelist[i].from == m.from && movelist[i].to == m.to)
		{
			bool result = true;
			move_make(movelist[i]);
			if (isAttacked(board.stm, board.king_loc[!board.stm]))
				result = false;
			move_unmake(movelist[i]);
			return result;
		}
	return false;
}

/******************************************************************************
*  This is not yet proper static exchange evaluation, but an approximation    *
*  proposed by Harm Geert Mueller under the acronym BLIND (better, or lower   *
*  if not defended. As the name indicates, it detects only obviously good     *
*  captures, but it seems enough to improve move ordering.                    *
******************************************************************************/

bool Blind(smove move)
{
	int sq_to = move.to;
	int sq_fr = move.from;
	int pc_fr = board.pieces[sq_fr];
	int pc_to = board.pieces[sq_to];
	int val = e.SORT_VALUE[pc_to];
	/* captures by pawn do not lose material */
	if (pc_fr == PAWN)
		return true;
	/* Captures "lower takes higher" (as well as BxN) are good by definition. */
	if (e.SORT_VALUE[pc_to] >= e.SORT_VALUE[pc_fr] - 50)
		return true;
	/* Make the first capture, so that X-ray defender show up*/
	ClearSq(sq_fr);
	/* Captures of undefended pieces are good by definition */
	if (!isAttacked(board.stm ^ 1, sq_to))
	{
		FillSq(board.stm, pc_fr, sq_fr);
		return true;
	}
	FillSq(board.stm, pc_fr, sq_fr);
	return false; // of other captures we know nothing, Jon Snow!
}

//returns movecount
U8 movegen(smove* moves, U8 tt_move)
{
	m = moves;
	movecount = 0;
	//Castling
	if (board.stm == WHITE) {
		if (board.castle & CASTLE_WK) {
			if ((board.pieces[F1] == PIECE_EMPTY)
				&& (board.pieces[G1] == PIECE_EMPTY)
				&& (!isAttacked((char)!board.stm, E1))
				&& (!isAttacked((char)!board.stm, F1))
				&& (!isAttacked((char)!board.stm, G1)))
				movegen_push(E1, G1, KING, PIECE_EMPTY, MFLAG_CASTLE);
		}
		if (board.castle & CASTLE_WQ) {
			if ((board.pieces[B1] == PIECE_EMPTY)
				&& (board.pieces[C1] == PIECE_EMPTY)
				&& (board.pieces[D1] == PIECE_EMPTY)
				&& (!isAttacked((char)!board.stm, E1))
				&& (!isAttacked((char)!board.stm, D1))
				&& (!isAttacked((char)!board.stm, C1)))
				movegen_push(E1, C1, KING, PIECE_EMPTY, MFLAG_CASTLE);
		}
	}
	else {
		if (board.castle & CASTLE_BK) {
			if ((board.pieces[F8] == PIECE_EMPTY)
				&& (board.pieces[G8] == PIECE_EMPTY)
				&& (!isAttacked((char)!board.stm, E8))
				&& (!isAttacked((char)!board.stm, F8))
				&& (!isAttacked((char)!board.stm, G8)))
				movegen_push(E8, G8, KING, PIECE_EMPTY, MFLAG_CASTLE);
		}
		if (board.castle & CASTLE_BQ) {
			if ((board.pieces[B8] == PIECE_EMPTY)
				&& (board.pieces[C8] == PIECE_EMPTY)
				&& (board.pieces[D8] == PIECE_EMPTY)
				&& (!isAttacked((char)!board.stm, E8))
				&& (!isAttacked((char)!board.stm, D8))
				&& (!isAttacked((char)!board.stm, C8)))
				movegen_push(E8, C8, KING, PIECE_EMPTY, MFLAG_CASTLE);
		}
	}
	for (S8 sq = 0; sq < 120; sq++)
	{
		if (board.color[sq] == board.stm)
		{
			if (board.pieces[sq] == PAWN)
			{
				movegen_pawn_move(sq, 0);
				movegen_pawn_capt(sq);
			}
			else
			{
				assert(board.pieces[sq] < (sizeof vectors / sizeof vectors[0]) && board.pieces[sq] >= 0);
				for (char dir = 0; dir < vectors[board.pieces[sq]]; dir++)
				{
					for (char pos = sq;;)
					{
						pos = pos + vector[board.pieces[sq]][dir];
						if (!IS_SQ(pos))
							break;
						if (board.pieces[pos] == PIECE_EMPTY)
							movegen_push(sq, pos, board.pieces[sq], PIECE_EMPTY, MFLAG_NORMAL);
						else
						{
							if (board.color[pos] != board.stm)
								movegen_push(sq, pos, board.pieces[sq], board.pieces[pos], MFLAG_CAPTURE);
							break; // we're hitting a piece, so looping is over
						}
						if (!slide[board.pieces[sq]])
							break;
					}
				}
			}
		}
	}
	/* if we have a best-move fed into movegen(), then increase its score */
	if ((tt_move != -1) && (tt_move < movecount)) moves[tt_move].score = SORT_HASH;
	return movecount;
}

U8 movegen_qs(smove* moves)
{
	m = moves;
	movecount = 0;
	for (S8 sq = 0; sq < 120; sq++)
	{
		if (board.color[sq] == board.stm)
		{
			if (board.pieces[sq] == PAWN)
			{
				movegen_pawn_move(sq, 1);
				movegen_pawn_capt(sq);
			}
			else
			{
				assert(board.pieces[sq] < (sizeof vectors / sizeof vectors[0]) && board.pieces[sq] >= 0);
				for (char dir = 0; dir < vectors[board.pieces[sq]]; dir++)
				{
					for (char pos = sq;;)
					{
						pos = pos + vector[board.pieces[sq]][dir];
						if (!IS_SQ(pos))
							break;
						if (board.pieces[pos] != PIECE_EMPTY)
						{
							if (board.color[pos] != board.stm)
								movegen_push(sq, pos, board.pieces[sq], board.pieces[pos], MFLAG_CAPTURE);
							break; // we're hitting a piece, so looping is over
						}
						if (!slide[board.pieces[sq]]) break;
					}
				}
			}
		}
	}
	return movecount;
}


void movegen_pawn_move(SQ sq, bool promotion_only)
{
	if (board.stm == WHITE)
	{
		if (promotion_only && (ROW(sq) != ROW_7))
			return;
		if (board.pieces[sq + NORTH] == PIECE_EMPTY)
		{
			movegen_push(sq, sq + NORTH, PAWN, PIECE_EMPTY, MFLAG_NORMAL);
			if ((ROW(sq) == ROW_2) && (board.pieces[sq + NN] == PIECE_EMPTY))
				movegen_push(sq, sq + NN, PAWN, PIECE_EMPTY, MFLAG_EP);
		}
	}
	else
	{
		if (promotion_only && (ROW(sq) != ROW_2))
			return;
		if (board.pieces[sq + SOUTH] == PIECE_EMPTY)
		{
			movegen_push(sq, sq + SOUTH, PAWN, PIECE_EMPTY, MFLAG_NORMAL);
			if ((ROW(sq) == ROW_7) && (board.pieces[sq + SS] == PIECE_EMPTY))
				movegen_push(sq, sq + SS, PAWN, PIECE_EMPTY, MFLAG_EP);
		}
	}
}

void movegen_pawn_capt(SQ sq)
{
	if (board.stm == WHITE) {
		if (IS_SQ(sq + NW) && ((board.ep == sq + NW) || (board.color[sq + NW] == (board.stm ^ 1)))) {
			movegen_push(sq, sq + NW, PAWN, board.pieces[sq + NW], MFLAG_CAPTURE);
		}
		if (IS_SQ(sq + NE) && ((board.ep == sq + NE) || (board.color[sq + NE] == (board.stm ^ 1)))) {
			movegen_push(sq, sq + 17, PAWN, board.pieces[sq + NE], MFLAG_CAPTURE);
		}
	}
	else {
		if (IS_SQ(sq + SE) && ((board.ep == sq + SE) || (board.color[sq + SE] == (board.stm ^ 1)))) {
			movegen_push(sq, sq + SE, PAWN, board.pieces[sq + SE], MFLAG_CAPTURE);
		}
		if (IS_SQ(sq + SW) && ((board.ep == sq + SW) || (board.color[sq + SW] == (board.stm ^ 1)))) {
			movegen_push(sq, sq + SW, PAWN, board.pieces[sq + SW], MFLAG_CAPTURE);
		}
	}
}

void movegen_push(char from, char to, U8 piece_from, U8 piece_cap, char flags)
{
	m[movecount].from = from;
	m[movecount].to = to;
	m[movecount].piece_from = piece_from;
	m[movecount].piece_to = piece_from;
	m[movecount].piece_cap = piece_cap;
	m[movecount].flags = flags;
	m[movecount].ply = board.ply;
	m[movecount].castle = board.castle;
	m[movecount].ep = board.ep;
	m[movecount].id = movecount;

	/**************************************************************************
	* Quiet moves are sorted by history score.                                *
	**************************************************************************/

	m[movecount].score = sd.history[board.stm][from][to];

	/**************************************************************************
	* Score for captures: add the value of the captured piece and the id      *
	* of the attacking piece. If two pieces attack the same target, the one   *
	* with the higher id (eg. Pawn=5) gets searched first. En passant gets    *
	* the same score as pawn takes pawn. Good captures are put at the front   *
	* of the list, bad captures - after ordinary moves.                       *
	**************************************************************************/

	if (piece_cap != PIECE_EMPTY) {
		if (Blind(m[movecount]) == 0) m[movecount].score = e.SORT_VALUE[piece_cap] + piece_from;
		else                          m[movecount].score = SORT_CAPT + e.SORT_VALUE[piece_cap] + piece_from;
	}

	if ((piece_from == PAWN) && (to == board.ep)) {
		m[movecount].score = SORT_CAPT + e.SORT_VALUE[PAWN] + 5;
		m[movecount].flags = MFLAG_EPCAPTURE;
	}

	/**************************************************************************
	* Put all four possible promotion moves on the list and score them.       *
	**************************************************************************/
	if ((piece_from == PAWN) && ((ROW(to) == ROW_1) || (ROW(to) == ROW_8)))
	{
		m[movecount].flags |= MFLAG_PROMOTION;
		for (char prompiece = QUEEN; prompiece <= KNIGHT; prompiece++)
		{
			m[movecount + prompiece - 1] = m[movecount];
			m[movecount + prompiece - 1].piece_to = prompiece;
			m[movecount + prompiece - 1].score += SORT_PROM + e.SORT_VALUE[prompiece];
			m[movecount + prompiece - 1].id = movecount + prompiece - 1;
		}
		movecount += 3;
	}
	movecount++;
}

void movegen_sort(U8 movecount, smove* m, U8 current)
{
	//find the move with the highest score - hoping for an early cutoff
	int high = current;
	for (int i = current + 1; i < movecount; i++)
		if (m[i].score > m[high].score)
			high = i;
	smove temp = m[high];
	m[high] = m[current];
	m[current] = temp;
}