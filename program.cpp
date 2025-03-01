#include "program.h"


#include "transposition.h"
#include "board.h"
#include "input.h"
#include "uci.h"

using namespace std;

#define MONTH (\
  __DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? "01" : "06") \
: __DATE__ [2] == 'b' ? "02" \
: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? "03" : "04") \
: __DATE__ [2] == 'y' ? "05" \
: __DATE__ [2] == 'l' ? "07" \
: __DATE__ [2] == 'g' ? "08" \
: __DATE__ [2] == 'p' ? "09" \
: __DATE__ [2] == 't' ? "10" \
: __DATE__ [2] == 'v' ? "11" \
: "12")
#define DAY (std::string(1,(__DATE__[4] == ' ' ?  '0' : (__DATE__[4]))) + (__DATE__[5]))
#define YEAR ((__DATE__[7]-'0') * 1000 + (__DATE__[8]-'0') * 100 + (__DATE__[9]-'0') * 10 + (__DATE__[10]-'0') * 1)

sSearchDriver sd = {};
s_options options;


U64 perft(U8 depth)
{
	U64 nodes = 0;
	if (depth == 0)
		return 1;
	smove m[256];
	int mcount = movegen(m, 0xFF);
	for (int i = 0; i < mcount; i++)
	{
		move_make(m[i]);
		if (!isAttacked(board.stm, board.king_loc[board.stm ^ 1]))
			nodes += perft(depth - 1);
		move_unmake(m[i]);
	}
	return nodes;
}

void util_perft(char* command)
{
	unsigned int starttime = gettime();
	int depth = 0;
	int converted = sscanf(command + 6, "%d", &depth);
	if (converted < 0)
		depth = 5;
	printf("Performance Test\n");
	unsigned int time = 0;
	U64 nodes = 0;
	for (U8 d = 1; d <= depth; d++)
	{
		nodes = perft(d);
		time = gettime() - starttime;
		printf("%d:\t%d\t%llu\n", (int)d, time, nodes);
	}
	printStats(time, nodes, 0);
}

void util_bench(char* command)
{
	unsigned int starttime = gettime();
	int depth = 0;
	int converted = sscanf(command + 6, "%d", &depth);
	if (converted < 0)
		depth = 8;
	printf("Benchmark Test\n");
	printSearchHeader();
	chronos.timeOver = false;
	chronos.post = false;
	chronos.ponder = false;
	sd.myside = board.stm;
	sd.starttime = gettime();
	sd.depth = 0;
	sd.nodes = 0;
	chronos.post = false;
	for (int n = 1; n <= depth; n++) {
		chronos.depth = n;
		chronos.flags = FDEPTH;
		search_run();
		U32 time = gettime() - sd.starttime;
		char buffer[2048];
		sprintf(buffer, " %2d. %9llu  %5d %5d %s", n, sd.nodes, time, sd. score, sd.pv);
		printf("%s\n", buffer);
	}
	if (gettime() - starttime < 1000) starttime = gettime() - 1000;
	unsigned int time = gettime() - starttime;
	printStats(time, sd.nodes, sd.q_nodes);
}

smove strToMove(char* a)
{
	smove m = {};
	m.from = convert_a_0x88(a);
	m.to = convert_a_0x88(a + 2);

	m.piece_from = board.pieces[m.from];
	m.piece_to = board.pieces[m.from];
	m.piece_cap = board.pieces[m.to];

	m.flags = 0;
	m.castle = 0;
	m.ep = -1;
	m.ply = 0;
	m.score = 0;

	/* default promotion to queen */

	if ((m.piece_to == PAWN) &&
		(ROW(m.to) == ROW_1 || ROW(m.to) == ROW_8))
		m.piece_to = QUEEN;


	switch (a[4]) {
	case 'q':
		m.piece_to = QUEEN;
		a++;
		break;
	case 'r':
		m.piece_to = ROOK;
		a++;
		break;
	case 'b':
		m.piece_to = BISHOP;
		a++;
		break;
	case 'n':
		m.piece_to = KNIGHT;
		a++;
		break;
	}

	//castling
	if ((m.piece_from == KING) &&
		((m.from == E1 && (m.to == G1 || m.to == C1)) ||
			(m.from == E8 && (m.to == G8 || m.to == C8)))) {
		m.flags = MFLAG_CASTLE;
	}

	/* ep
		if the moving-piece is a Pawn, the square it moves to is empty and
		it was a diagonal move it has to be an en-passant capture.
	*/
	if ((m.piece_from == PAWN) &&
		(m.piece_cap == PIECE_EMPTY) &&
		((abs(m.from - m.to) == 15) || (abs(m.from - m.to) == 17))) {
		m.flags = MFLAG_EPCAPTURE;
	}

	if ((m.piece_from == PAWN) && (abs(m.from - m.to) == 32)) {
		m.flags |= MFLAG_EP;
	}

	return m;
}

bool algebraic_moves(char* a)
{
	smove m = {};
	bool found_match = false;
	while (a[0]) {

		if (!((a[0] >= 'a') && (a[0] <= 'h'))) {
			a++;
			continue;
		}
		m = strToMove(a);
		found_match = move_isLegal(m);
		if (found_match)
		{
			move_make(m);

			if ((m.piece_from == PAWN) ||
				(move_iscapt(m)) ||
				(m.flags == MFLAG_CASTLE))
				board.rep_index = 0;
		}
		else
			break;
		a += 4;
		if (a[0] == 0) break;
		if (a[0] != ' ') a++;
	}
	return found_match;
}


char* algebraic_writemove(smove m, char* a)
{
	char parray[5] = { 0,'q','r','b','n' };
	convert_0x88_a(m.from, a);
	convert_0x88_a(m.to, a + 2);
	a += 4;
	if (m.piece_to != m.piece_from) {
		a[0] = parray[m.piece_to];
		a++;
	}
	a[0] = 0;
	return a;
}

void convert_0x88_a(SQ sq, char* a)
{
	a[0] = COL(sq) + 'a';
	a[1] = ROW(sq) + '1';
	a[2] = 0;
}

SQ convert_a_0x88(char* a)
{
	return a[0] - 'a' | ((a[1] - '1') << 4);
}

void PrintWelcome() {
	cout << "RapCpw " << YEAR <<"-"<< MONTH <<"-" << DAY << endl;
}

void printStats(unsigned int time,unsigned long long nodes,unsigned long long qnodes)
{
    nodes += (nodes == 0);
    printf("-----------------------------\n");
    printf("Time        : %d \n", time);
    printf("Nodes       : %llu \n", nodes);
	printf("Nps         : %llu\n", (nodes * 1000) / time);
	if (qnodes)
	{
		printf("Quiesc nodes: %llu \n", qnodes);
		printf("Ratio       : %llu %\n", qnodes * 100 / nodes);
	}
    printf("-----------------------------\n");
}

void printSearchHeader()
{
    printf("-------------------------------------------------------\n");
    printf("ply      nodes   time score pv\n");
    printf("-------------------------------------------------------\n");
}

int main()
{
	PrintWelcome();
	InputInit();
	setDefaultEval();
	tt_init();
	tt_setsize(0x4000000);     //64m
	ttpawn_setsize(0x1000000); //16m
	tteval_setsize(0x2000000); //32m
	board_loadFromFen(STARTFEN);
	UciLoop();
}