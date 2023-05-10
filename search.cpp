#include "search.h"


/* symbols used to enhance readability */
#define DO_NULL    1
#define NO_NULL    0
#define IS_PV      1
#define NO_PV      0
#define TIMEBUFFER 500
#define MOVESTOGO 24

using namespace std;

U8 bestmove;         // move id passed between iterations for sorting purposes
smove move_to_make;	 // move to be returned when search runs out of time
smove move_to_ponder;//last ponder move
schronos chronos = {};

unsigned int gettime() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return (unsigned int)((((U64)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000);
}

void time_calc_movetime() {

	/* no movetime to be calculated in these search types */

	if (chronos.flags & (FINFINITE | FDEPTH | FNODES))
		return;

	/* if the movetime is given */

	if (chronos.flags & FMOVETIME) {
		if (chronos.movetime > TIMEBUFFER) {
			sd.movetime = chronos.movetime - TIMEBUFFER;
		}
		else {
			sd.movetime = -1;
		}
		return;
	}

	/* in any other case we are given an ordinary timecontrol
	   (time + opt. movestogo + opt. incremental)

		we take the total time left (chronos.time[sd.myside]), divide it
		by the number of moves that are still to play (chronos.movestogo)
		and we have the allowed time per move.
		If we are given an incremental time control, we consider this
		here as well.

		In case we are not told how many moves we are supposed to play
		with the time left, we assume a constant. This results in a
		slower play in the beginning and more rapid towards the end.

		as a little buffer we always assume that there are more moves
		to be played than we actually have to. This should avoid loosing on time.
	*/

	sd.movetime = 0;
	int movestogo = MOVESTOGO;
	if (chronos.flags & FMOVESTOGO)
		movestogo = chronos.movestogo + 2;
	if (chronos.time[sd.myside] < 0)
		chronos.time[sd.myside] = 0;
	if (chronos.inc[sd.myside] < 0)
		chronos.inc[sd.myside] = 0;
	if (chronos.flags & FTIME)
		sd.movetime += chronos.time[sd.myside] / movestogo;
	if (chronos.flags & FINC)
		sd.movetime += chronos.inc[sd.myside];
	if (sd.movetime > TIMEBUFFER)
		sd.movetime -= TIMEBUFFER;
	else
		sd.movetime = -1;
}

bool time_stop_root() {
	std::string input;
	if (GetInput(input))
		UciCommand((char*)input.c_str());
	if (chronos.timeOver)
		return true;
	if (chronos.flags & FINFINITE)
		return false;
	if (chronos.flags & FDEPTH)
		return (sd.depth > chronos.depth);
	if (chronos.flags & FNODES)
		return (sd.nodes > chronos.nodes);
	if (chronos.flags & FMOVETIME)
		return ((gettime() - sd.starttime) > sd.movetime);
	return ((gettime() - sd.starttime) * 2) > sd.movetime;
}

bool time_stop() {

	if (sd.depth <= 1)
		return false;
	std::string input;
	if (GetInput(input))
		UciCommand((char*)input.c_str());
	if (chronos.timeOver)
		return true;
	if (chronos.flags & FINFINITE)
		return false;
	if (chronos.flags & FDEPTH)
		return sd.depth > chronos.depth;
	if (chronos.flags & FNODES)
		return sd.nodes > chronos.nodes;
	if (chronos.flags & FMOVETIME)
		return (gettime() - sd.starttime) > chronos.movetime;

	/* again we are not doing one of the easier to manage time controls

		other than the function before we are now already in the middle of
		a search. Actually if our prediction was right, we wouldn't have
		entered this ply if we weren't able to finish it.

		so ( (gettime() - sd.starttime) < movetime ) should always be given.

		Anyway there are some situations where our prediction method fails.
		The problem is that should we return true here and stop the search
		a lot of information gathered gets useless, as the different lines
		searched are not verfied.

		if possible give in such cases a little overdraft and hope that
		the misprediction was only small
	*/

	if ((int)(gettime() - sd.starttime) > sd.movetime) {

		int movestogo = MOVESTOGO;
		if (chronos.flags & FMOVESTOGO) movestogo = chronos.movestogo;

		if ((movestogo > 5) &&
			((int)(gettime() - sd.starttime) < (sd.movetime * 2)) &&
			(sd.movetime > 5000)) {
			return false;
		}
		else
			return true;
	}
	return false;
}

bool isAttacked(char byColor, SQ sq) {

	/* pawns */
	if (byColor == WHITE && board.pawn_ctrl[WHITE][sq])
		return true;

	if (byColor == BLACK && board.pawn_ctrl[BLACK][sq])
		return true;

	/* knights */
	if (leaperAttack(byColor, sq, KNIGHT))
		return true;

	/* kings */
	if (leaperAttack(byColor, sq, KING))
		return true;

	/* straight line sliders */
	if (straightAttack(byColor, sq, NORTH)
		|| straightAttack(byColor, sq, SOUTH)
		|| straightAttack(byColor, sq, EAST)
		|| straightAttack(byColor, sq, WEST))
		return true;

	/* diagonal sliders */
	if (diagAttack(byColor, sq, NE)
		|| diagAttack(byColor, sq, SE)
		|| diagAttack(byColor, sq, NW)
		|| diagAttack(byColor, sq, SW))
		return true;
	return false;
}

bool leaperAttack(char byColor, SQ sq, char byPiece)
{
	S8 nextSq;
	for (int dir = 0; dir < 8; dir++)
	{
		nextSq = sq + vector[byPiece][dir];
		if (IS_SQ(nextSq) && isPiece(byColor, byPiece, nextSq))
			return true;
	}
	return false;
}

bool straightAttack(char byColor, SQ sq, int vect)
{
	int nextSq = sq + vect;
	while (IS_SQ(nextSq))
	{
		if (board.color[nextSq] != COLOR_EMPTY)
			return (board.color[nextSq] == byColor) && (board.pieces[nextSq] == ROOK || board.pieces[nextSq] == QUEEN);
		nextSq = nextSq + vect;
	}
	return false;
}

bool diagAttack(int byColor, SQ sq, int vect)
{
	int nextSq = sq + vect;
	while (IS_SQ(nextSq))
	{
		if (board.color[nextSq] != COLOR_EMPTY)
			return (board.color[nextSq] == byColor) && (board.pieces[nextSq] == BISHOP || board.pieces[nextSq] == QUEEN);
		nextSq = nextSq + vect;
	}
	return false;
}

bool bishAttack(int byColor, SQ sq, int vect)
{
	int nextSq = sq + vect;
	while (IS_SQ(nextSq))
	{
		if (board.color[nextSq] != COLOR_EMPTY)
		{
			if (board.color[nextSq] == byColor
				&& board.pieces[nextSq] == BISHOP)
				return true;
			return false;
		}
		nextSq = nextSq + vect;
	}
	return false;
}

/* retrieving pv from hash table */
void GetPv(char* pv)
{
	sboard rootb = board;
	char best;
	smove moves[256];
	int mcount = 0;
	move_to_ponder = {};
	for (U8 depth = 1; depth <= sd.depth; depth++)
	{
		best = -1;
		tt_probe(0, 0, 0, &best);
		if (best == -1)
			break;
		mcount = movegen(moves, 0xFF);
		for (int i = 0; i < mcount; i++)
		{
			if (moves[i].id == best)
			{
				if (depth == 2)
					move_to_ponder = moves[i];
				move_make(moves[i]);
				pv = algebraic_writemove(moves[i], pv);
				pv[0] = ' ';
				pv++;
				break;
			}
		}
	}
	pv[0] = 0;
	board = rootb;
}

int Quiesce(int alpha, int beta)
{
	CheckInput();
	if (chronos.timeOver)
		return 0;

	sd.nodes++;
	sd.q_nodes++;

	/* get a "stand pat" score */
	int val = eval(alpha, beta, 1);
	int stand_pat = val;

	/* check if stand-pat score causes a beta cutoff */
	if (val >= beta)
		return beta;

	/* check if stand-pat score may become a new alpha */
	if (alpha < val)
		alpha = val;

	/**************************************************************************
	*  We have taken into account the stand pat score, and it didn't let us   *
	*  come to a definite conclusion about the position. So we have to search *
	**************************************************************************/

	smove movelist[256];
	U8 mcount = movegen_qs(movelist);

	for (U8 i = 0; i < mcount; i++) {

		movegen_sort(mcount, movelist, i);

		if (movelist[i].piece_cap == KING) return INF;

		/**********************************************************************
		*  Delta cutoff - a move guarentees the score well below alpha, so    *
		*  there's no point in searching it. We don't use his heuristic in    *
		*  the endgame, because of the insufficient material issues.          *
		**********************************************************************/

		if ((stand_pat + e.PIECE_VALUE[movelist[i].piece_cap] + 200 < alpha)
			&& (board.piece_material[!board.stm] - e.PIECE_VALUE[movelist[i].piece_cap] > e.ENDGAME_MAT)
			&& (!move_isprom(movelist[i])))
			continue;

		/**********************************************************************
		*  badCapture() replaces a cutoff based on the Static Exchange Evalu- *
		*  ation, marking the place where it ought to be coded. Despite being *
		*  just a hack, it saves quite a few nodes.                           *
		**********************************************************************/

		if (badCapture(movelist[i])
			&& !move_canSimplify(movelist[i])
			&& !move_isprom(movelist[i]))
			continue;

		/**********************************************************************
		*  Cutoffs  misfired, we have to search the current move              *
		**********************************************************************/

		move_make(movelist[i]);
		val = -Quiesce(-beta, -alpha);
		move_unmake(movelist[i]);

		if (chronos.timeOver)
			return 0;

		if (val > alpha) {
			if (val >= beta) return beta;
			alpha = val;
		}
	}
	return alpha;
}

bool badCapture(smove move) {

	/* captures by pawn do not lose material */
	if (move.piece_from == PAWN)
		return false;

	/* Captures "lower takes higher" (as well as BxN) are good by definition. */
	if (e.PIECE_VALUE[move.piece_cap] >= e.PIECE_VALUE[move.piece_from] - 50)
		return false;

	/**************************************************************************
	*   When the enemy piece is defended by a pawn, in the quiescence search  *
	*   we  will  accept rook takes minor, but not minor takes pawn. ( More   *
	*   exact  version  should accept B/N x P if (a) the pawn  is  the  sole  *
	*   defender and (b) there is more than one attacker.                     *
	**************************************************************************/

	if (board.pawn_ctrl[board.color[move.from] ^ 1][move.to]
		&& e.PIECE_VALUE[move.piece_cap] + 200 < e.PIECE_VALUE[move.piece_from])
		return true;

	if (e.PIECE_VALUE[move.piece_cap] + 500 < e.PIECE_VALUE[move.piece_from]) {
		if (leaperAttack(board.color[move.from] ^ 1, move.to, KNIGHT)) return true;
		if (bishAttack(board.color[move.from] ^ 1, move.to, NE)) return true;
		if (bishAttack(board.color[move.from] ^ 1, move.to, NW)) return true;
		if (bishAttack(board.color[move.from] ^ 1, move.to, SE)) return true;
		if (bishAttack(board.color[move.from] ^ 1, move.to, SW)) return true;
	}

	/* if a capture is not processed, it cannot be considered bad */
	return false;
}

/******************************************************************************
*  search_run() is the only function called outside search.cpp, so it acts as *
*  an interface. After some preparatory work it calls search_iterate();       *
******************************************************************************/

void search_run()
{
	search_clearDriver();
	time_calc_movetime();
	ageHistoryTable();
	search_iterate();
}

void search_clearDriver() {
	sd.myside = board.stm;         // remember color - needed in contempt()
	sd.starttime = gettime();
	sd.movetime = 0;
	sd.depth = 0;
	sd.nodes = 0;
	sd.q_nodes = 0;
}

/******************************************************************************
*  search_iterate() calls search_root() with increasing depth until allocated *
*  time is exhausted.                                                         *
******************************************************************************/

void search_iterate() {
	int val;

	// check the exact number of legal moves in the current position
	int move_count = move_countLegal();

	// do a full-window 1-ply search to get the first estimate of val
	sd.depth = 1;
	val = search_root(sd.depth, -INF, INF);

	// main loop, increasing deph in steps of 1

	for (sd.depth = 2; sd.depth <= MAX_DEPTH; sd.depth++) {

		// breaking conditions - either expired time
		// or just one legal reply and position searched to depth 4

		if (time_stop_root())
			break;
		if (move_count == 1 && sd.depth == 5)
			break;

		// this function deals with aspiration window
		val = search_widen(sd.depth, val);
	}
	// after the loop has finished, send the move to the interface
	if (chronos.post)
		Bestmove();
}

int search_widen(int depth, int val) {
	int temp = val,
		alpha = val - options.aspiration,
		beta = val + options.aspiration;

	temp = search_root(sd.depth, alpha, beta);
	if (temp <= alpha || temp >= beta)
		temp = search_root(sd.depth, -INF, INF);
	return temp;
}

int search_root(U8 depth, int alpha, int beta) {

	int flagInCheck;
	smove movelist[256];
	int val = 0;
	int best = -INF;

	U8 currmove_legal = 0;

	/* Check  extension is done also at  the  root */
	flagInCheck = isAttacked(board.stm ^ 1, board.king_loc[board.stm]);
	if (flagInCheck) ++depth;

	U8 mcount = movegen(movelist, bestmove);

	for (U8 i = 0; i < mcount; i++) {

		int cl = board.stm;
		movegen_sort(mcount, movelist, i);

		if (movelist[i].piece_cap == KING) {
			alpha = INF;
			bestmove = movelist[i].id;
		}

		move_make(movelist[i]);

		// filter out illegal moves
		if (isAttacked(board.stm, board.king_loc[!board.stm])) {
			move_unmake(movelist[i]);
			continue;
		}

		sd.cutoff[cl][movelist[i].from][movelist[i].to] -= 1;

		//	if ( mode == PROTO_UCI && depth > 6)
		//		info_currmove( movelist[i], currmove_legal );

		currmove_legal++;

		/* the "if" clause introduces PVS at root */

		if (best == -INF)
			val = -Search(depth - 1, 0, -beta, -alpha, DO_NULL, IS_PV);
		else
		{
			val = -Search(depth - 1, 0, -alpha - 1, -alpha, DO_NULL, NO_PV);
			if (val > alpha)
				val = -Search(depth - 1, 0, -beta, -alpha, DO_NULL, IS_PV);
		}

		if (val > best) best = val;
		move_unmake(movelist[i]);

		if (chronos.timeOver)
			break;

		if (val > alpha) {

			bestmove = movelist[i].id;
			move_to_make = movelist[i];

			if (val > beta) {
				tt_save(depth, beta, TT_BETA, bestmove);
				info_pv(beta);
				return beta;
			}

			alpha = val;
			tt_save(depth, alpha, TT_ALPHA, bestmove);

			info_pv(val);
		}
	}
	tt_save(depth, alpha, TT_EXACT, bestmove);
	return alpha;
}

int Search(U8 depth, U8 ply, int alpha, int beta, int can_null, int is_pv)
{
	int  val = -INF;
	char bestmove;
	char tt_move_index = (char)-1;
	char tt_flag = TT_ALPHA;
	int  flagInCheck;
	int  raised_alpha = 0;
	int  f_prune = 0;
	int  reduction_depth = 0;
	int  moves_tried = 0;
	int  new_depth;
	int  mate_value = INF - ply; // will be used in mate distance pruning
	smove movelist[256];         // move list
	smove move;                  // current move

	/**************************************************************************
	*  Probably later we will want to probe the transposition table. Here we  *
	*  tell  the  cpu to prepare for that event. This is just a minor  speed  *
	*  optimization and program would run fine without that.                  *
	**************************************************************************/

	_mm_prefetch((char*)&tt[board.hash & tt_size], _MM_HINT_NTA);

	/**************************************************************************
	* Check for timeout. This is quite time-consuming, so we do it only every *
	* every so often. The side effect is that if we want to limit search by   *
	* the number of nodes, it will be slightly inexact.                       *
	**************************************************************************/

	CheckInput(); // check for new commands or timeout
	if (chronos.timeOver)
		return 0;

	/**************************************************************************
	* MATE DISTANCE PRUNING, a minor improvement that helps to shave off some *
	* some nodes when the checkmate is near. Basically it prevents looking    *
	* for checkmates taking longer than one we have already found. No Elo     *
	* gain expected, but it's a nice feature. Don't use it at the root,       *
	* since  this code  doesn't return a move, only a value.                  *
	**************************************************************************/
	if (alpha < -mate_value) alpha = -mate_value;
	if (beta > mate_value - 1) beta = mate_value - 1;
	if (alpha >= beta) return alpha;
	/**************************************************************************
	*  Are we in check? If so, extend. It also means that program will never  *
	*  never enter quiescence search while in check.                          *
	**************************************************************************/
	flagInCheck = (isAttacked(board.stm ^ 1, board.king_loc[board.stm]));
	if (flagInCheck)
		depth += 1;
	/**************************************************************************
	*  At leaf nodes we do quiescence search (captures only) to make sure     *
	*  that only relatively quiet positions with no hanging pieces will be    *
	*  evaluated.                                                             *
	**************************************************************************/
	if (depth < 1)
		return Quiesce(alpha, beta);
	sd.nodes++;
	if (isRepetition()) return Contempt();
	/**************************************************************************
	*  Read the transposition table. We may have already searched current     *
	*  position. If depth was sufficient, then we might use the score         *
	*  of that search. If not, hash move still is expected to be good         *
	*  and should be sorted first.                                            *
	*                                                                         *
	*  NOTE: current implementation is sub-standard, since tt_move is just    *
	*  an index showing move's location on a move list. We should be able     *
	*  to retrieve move without generating full move list instead.            *
	**************************************************************************/
	if ((val = tt_probe(depth, alpha, beta, &tt_move_index)) != INVALID) {
		// in pv nodes we return only in case of an exact hash hit
		if (!is_pv || (val > alpha && val < beta)) {

			/******************************************************************
			*  Here we must be careful about checkmate scoring. "Mate in n"   *
			*  returned by transposition table means "mate in n if we start   *
			*  counting n right now". Yet search always returns mate scores   *
			*  as distance from root, so we must convert to that metric.      *
			*  Other programs might hide similar code within tt_probe() and   *
			*  tt_save() functions.                                           *
			******************************************************************/

			if (abs(val) > INF - 100) {
				if (val > 0) val -= ply;
				else         val += ply;
			}

			return val;
		}
	}
	/**************************************************************************
	* EVAL PRUNING / STATIC NULL MOVE                                         *
	**************************************************************************/
	if (depth < 3
		&& !is_pv
		&& !flagInCheck
		&& abs(beta - 1) > -INF + 100)
	{
		int static_eval = eval(alpha, beta, 1);

		int eval_margin = 120 * depth;
		if (static_eval - eval_margin >= beta)
			return static_eval - eval_margin;
	}
	/**************************************************************************
	*  Here  we introduce  NULL MOVE PRUNING. It  means  allowing opponent    *
	*  to execute two moves in a row, i.e. capturing something and escaping   *
	*  a recapture. If this cannot  wreck our position, then it is so good    *
	*  that there's  no  point in searching further. The flag "can_null"      *
	*  ensures we don't do  two null moves in a row. Null move is not used    *
	*  in  the endgame because of the risk of zugzwang.                       *
	**************************************************************************/

	if (depth > 2
		&& can_null
		&& !is_pv
		&& eval(alpha, beta, 1) > beta
		&& board.piece_material[board.stm] > e.ENDGAME_MAT
		&& !flagInCheck)
	{
		char ep_old = board.ep;
		move_makeNull();

		/**********************************************************************
		*  We use so-called adaptative null move pruning. Size of reduction   *
		*  depends on remaining  depth.                                       *
		**********************************************************************/

		char R = 2;
		if (depth > 6) R = 3;

		val = -Search(depth - R - 1, ply + 1, -beta, -beta + 1, NO_NULL, NO_PV);

		move_unmakeNull(ep_old);

		if (chronos.timeOver)
			return 0;
		if (val >= beta) return beta;
	}   // end of null move code

	/**************************************************************************
	*  RAZORING - if a node is close to the leaf and its static score is low, *
	*  we drop directly to the quiescence search.                             *
	**************************************************************************/

	if (!is_pv && !flagInCheck && tt_move_index == -1 && can_null && depth <= 3)
	{
		int threshold = alpha - 300 - (depth - 1) * 60;
		if (eval(alpha, beta, 1) < threshold) {
			val = Quiesce(alpha, beta);
			if (val < threshold) return alpha;
		}
	} // end of razoring code

	/**************************************************************************
	*  Decide  if FUTILITY PRUNING  is  applicable. If we are not in check,   *
	*  not searching for a checkmate and eval is below (alpha - margin), it   *
	*  might  mean that searching non-tactical moves at low depths is futile  *
	*  so we set a flag allowing this pruning.                                *
	**************************************************************************/

	int fmargin[4] = { 0, 200, 300, 500 };

	if (depth <= 3
		&& !is_pv
		&& !flagInCheck
		&& abs(alpha) < 9000
		&& eval(alpha, beta, 1) + fmargin[depth] <= alpha)
		f_prune = 1;

	/**************************************************************************
	*  Generate moves, then place special cases higher on the list            *
	**************************************************************************/

	U8 mcount = movegen(movelist, tt_move_index);
	ReorderMoves(movelist, mcount, ply);
	bestmove = movelist[0].id;

	/**************************************************************************
	*  Loop through the move list, trying them one by one.                    *
	**************************************************************************/

	for (int i = 0; i < mcount; i++) {

		int cl = board.stm;
		movegen_sort(mcount, movelist, i); // pick the best of untried moves
		move = movelist[i];
		move_make(move);

		// filter out illegal moves
		if (isAttacked(board.stm, board.king_loc[!board.stm])) {
			move_unmake(move);
			continue;
		}
		/**********************************************************************
		*  When the futility pruning flag is set, prune moves which do not    *
		*  give  check and do not change material balance.  Some  programs    *
		*  prune insufficient captures as well, but that seems too risky.     *
		**********************************************************************/

		if (f_prune
			&& moves_tried
			&& !move_iscapt(move)
			&& !move_isprom(move)
			&& !isAttacked(board.stm ^ 1, board.king_loc[board.stm])) {
			move_unmake(move);
			continue;
		}

		sd.cutoff[cl][move.from][move.to] -= 1;
		moves_tried++;
		reduction_depth = 0;       // this move has not been reduced yet
		new_depth = depth - 1;     // decrease depth by one ply

		/**********************************************************************
		*  Late move reduction. Typically a cutoff occurs on trying one of    *
		*  the first moves. If it doesn't, we are probably in an all-node,    *
		*  which means that all moves will fail low. So we might as well      *
		*  spare some effort, searching to reduced depth. Of course this is   *
		*  not a foolproof method, but it works more often than not. Still,   *
		*  we  need to exclude certain moves from reduction, in  order  to    *
		*  filter out tactical moves that may cause a late cutoff.            *
		**********************************************************************/

		if (!is_pv
			&& new_depth > 3
			&& moves_tried > 3
			&& !isAttacked(board.stm ^ 1, board.king_loc[board.stm])
			&& !flagInCheck
			&& sd.cutoff[cl][move.from][move.to] < 50
			&& (move.from != sd.killers[0][ply].from || move.to != sd.killers[0][ply].to)
			&& (move.from != sd.killers[1][ply].from || move.to != sd.killers[1][ply].to)
			&& !move_iscapt(move)
			&& !move_isprom(move)) {

			/******************************************************************
			* Real programs tend to use more advanced formulas to calculate   *
			* reduction depth. Typically they calculate it from both remai-   *
			* ning depth and move count. Formula used here is very basic and  *
			* gives only a minimal improvement over uniform one ply reduction,*
			* and is included for the sake of completeness only.              *
			******************************************************************/

			sd.cutoff[cl][move.from][move.to] = 50;
			reduction_depth = 1;
			if (moves_tried > 6) reduction_depth += 1;
			new_depth -= reduction_depth;
		}
		/**********************************************************************
		*  The code below introduces principal variation search. It  means    *
		*  that once we are in a PV-node (indicated by IS_PV flag) and  we    *
		*  have  found a move that raises alpha, we assume that  the  rest    *
		*  of moves ought to be refuted. This is done  relatively  cheaply    *
		*  by using  a null-window search centered around alpha.  Only  if    *
		*  this search fails high, we are forced repeat it with full window.  *
		*                                                                     *
		*  Understanding the shorthand in the first two lines is a bit tricky *
		*  If alpha has not been raised, we might be either in a zero window  *
		*  (scout) node or in an open window (pv) node, entered after a scout *
		*  search failed high. In both cases, we need to search with the same *
		*  alpha, the same beta AND the same node type.                       *                                            *
		**********************************************************************/

		if (!raised_alpha)
			val = -Search(new_depth, ply + 1, -beta, -alpha, DO_NULL, is_pv);
		else
		{
			val = -Search(new_depth, ply + 1, -alpha - 1, -alpha, DO_NULL, NO_PV);
			if (val > alpha)
				val = -Search(new_depth, ply + 1, -beta, -alpha, DO_NULL, IS_PV);
		}
		/**********************************************************************
		*  Sometimes reduced search brings us above alpha. This is unusual,   *
		*  since we expected reduced move to be bad in first place. It is     *
		*  not certain now, so let's search to the full, unreduced depth.     *
		**********************************************************************/

		if (reduction_depth && val > alpha)
		{
			new_depth += reduction_depth;
			reduction_depth = 0;
			if (!raised_alpha)
				val = -Search(new_depth, ply + 1, -beta, -alpha, DO_NULL, is_pv);
			else
			{
				val = -Search(new_depth, ply + 1, -alpha - 1, -alpha, DO_NULL, NO_PV);
				if (val > alpha)
					val = -Search(new_depth, ply + 1, -beta, -alpha, DO_NULL, IS_PV);
			}
		}
		move_unmake(move);
		if (chronos.timeOver)
			return 0;
		/**********************************************************************
		*  We can improve over alpha, so we change the node value together    *
		*  with  the expected move. Also the raised_alpha flag, needed  to    *
		*  control PVS, is set. In case of a beta cuoff, when our position    *
		*  is  so good that the score will not be accepted one ply before,    *
		*  we return it immediately.                                          *
		**********************************************************************/
		if (val > alpha) {
			bestmove = movelist[i].id;
			sd.cutoff[cl][move.from][move.to] += 6;
			if (val >= beta) {
				/**************************************************************
				*  On a quiet move update killer moves and history table      *
				*  in order to enhance move ordering.                         *
				**************************************************************/

				if (!move_iscapt(move)
					&& !move_isprom(move)) {
					setKillers(movelist[i], ply);
					sd.history[board.stm][move.from][move.to] += depth * depth;

					/**********************************************************
					*  With super deep search history table would overflow    *
					*  - let's prevent it.                                    *
					**********************************************************/

					if (sd.history[board.stm][move.from][move.to] > SORT_KILL) {
						for (int cl = 0; cl < 2; cl++)
							for (int a = 0; a < 128; a++)
								for (int b = 0; b < 128; b++) {
									sd.history[cl][a][b] = sd.history[cl][a][b] / 2;
								}
					}
				}
				tt_flag = TT_BETA;
				alpha = beta;
				break; // no need to search any further
			}
			raised_alpha = 1;
			tt_flag = TT_EXACT;
			alpha = val;
		} // changing the node value is finished
	}   // end of looping through the moves

	/**************************************************************************
	*  Checkmate and stalemate detection: if we can't find a legal move in    *
	*  the current position, we test if we are in check. If so, mate score    *
	*  relative to search depth is returned. If not, we use draw score pro-   *
	*  vided by contempt() function.                                          *
	**************************************************************************/
	if (!moves_tried)
	{
		bestmove = -1;
		if (flagInCheck)
			alpha = -INF + ply;
		else
			alpha = Contempt();
	}
	/* tt_save() does not save anything when the search is timed out */
	tt_save(depth, alpha, tt_flag, bestmove);
	return alpha;
}

void setKillers(smove m, U8 ply)
{
	/* if a move isn't a capture, save it as a killer move */
	if (m.piece_cap == PIECE_EMPTY) {

		/* make sure killer moves will be different
		before saving secondary killer move */
		if (m.from != sd.killers[ply][0].from
			|| m.to != sd.killers[ply][0].to)
			sd.killers[ply][1] = sd.killers[ply][0];

		/* save primary killer move */
		sd.killers[ply][0] = m;
	}
}

void ReorderMoves(smove* m, U8 mcount, U8 ply) {

	for (int j = 0; j < mcount; j++) {
		if ((m[j].from == sd.killers[ply][1].from)
			&& (m[j].to == sd.killers[ply][1].to)
			&& (m[j].score < SORT_KILL - 1)) {
			m[j].score = SORT_KILL - 1;
		}

		if ((m[j].from == sd.killers[ply][0].from)
			&& (m[j].to == sd.killers[ply][0].to)
			&& (m[j].score < SORT_KILL)) {
			m[j].score = SORT_KILL;
		}
	}
}

void info_pv(int val)
{
	sd.score = val;
	char score[10];
	std::fill(std::begin(sd.pv), std::end(sd.pv), 0);
	if (abs(val) < INF - 2000)
		sprintf(score, "cp %d", val);
	else if (val > 0)
		sprintf(score, "mate %d", (INF - val) / 2 + 1);
	else
		sprintf(score, "mate %d", -(INF + val) / 2 - 1);
	U32 nodes = (U32)sd.nodes;
	U32 time = gettime() - sd.starttime;
	GetPv(sd.pv);
	if (chronos.post) {
		char buffer[2048];
		sprintf(buffer, "info depth %d score %s time %u nodes %u nps %u hashfull %llu pv %s", (int)sd.depth, score, time, nodes, countNps(nodes, time),ttPermill(), sd.pv);
		printf("%s\n", buffer);
	}
}

/******************************************************************************
*  countNps() guards against overflow and thus cares for displaying correct   *
*  nps during longer searches. Node count is converted from U64 to unsigned   *
*  int because of some problems with output.                                  *
******************************************************************************/
unsigned int countNps(unsigned int nodes, unsigned int time) {
	if (time == 0)
		return 0;
	if (time > 20000)
		return nodes / (time / 1000);
	else
		return (nodes * 1000) / time;
}

/******************************************************************************
*  Checking if the current position has been already encountered on the cur-  *
*  rent search path. Function does NOT check the number of repetitions.       *
******************************************************************************/
bool isRepetition()
{
	for (int i = 0; i < board.rep_index; i++)
		if (board.rep_stack[i] == board.hash)
			return true;
	return false;
}

/******************************************************************************
*  Clearing the history table is needed at the beginning of a search starting *
*  from a new position, like at the beginning of a new game.                  *
******************************************************************************/
void clearHistoryTable()
{
	for (int cl = 0; cl < 2; cl++)
		for (int i = 0; i < 128; i++)
			for (int j = 0; j < 128; j++)
			{
				sd.history[cl][i][j] = 0;
				sd.cutoff[cl][i][j] = 100;
			}
}

/******************************************************************************
* ageHistoryTable() is run between searches to decrease the history values    *
* used for move sorting. This  causes obsolete information to disappear gra-  *
* dually. Clearing the table was worse for the move ordering.                 *
******************************************************************************/

void ageHistoryTable()
{
	for (int cl = 0; cl < 2; cl++)
		for (int i = 0; i < 128; i++)
			for (int j = 0; j < 128; j++)
			{
				sd.history[cl][i][j] = sd.history[cl][i][j] / 8;
				sd.cutoff[cl][i][j] = 100;
			}
}

/******************************************************************************
*  contempt() returns a draw value (which may be non-zero) relative to the    *
*  side to move and to the  game  stage. This  way  we may make our program   *
*  play for a  draw  or strive to avoid it.                                   *
******************************************************************************/

int Contempt()
{
	int value = board.piece_material[sd.myside] < e.ENDGAME_MAT ? 0 : options.contempt;
	if (board.stm == sd.myside)
		return -value;
	else
		return value;
}

void CheckInput()
{
	if (!chronos.timeOver && !(sd.nodes & 4095))
		chronos.timeOver = time_stop();
}

void Bestmove()
{
	char make[6] = {};
	algebraic_writemove(move_to_make, make);
	if (options.ponder && (move_to_ponder.from != move_to_ponder.to))
	{
		char ponder[6] = {};
		algebraic_writemove(move_to_ponder, ponder);
		printf("bestmove %s ponder %s\n", make, ponder);
		return;
	}
	printf("bestmove %s\n", make);
}