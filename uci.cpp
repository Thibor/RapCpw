
#include "program.h"

using namespace std;

static void UciGo(char* command) {
	chronos.ponder = false;
	chronos.post = true;
	chronos.timeOver = false;
	char* token;

	chronos.flags = 0;

	if (strstr(command, "infinite"))
		chronos.flags |= FINFINITE;
	if (strstr(command, "ponder"))
		chronos.flags |= FINFINITE;
	int converted;
	token = strstr(command, "wtime");
	if (token > 0)
	{
		chronos.flags |= FTIME;
		converted = sscanf(token, "%*s %d", &chronos.time[WHITE]);
	}
	token = strstr(command, "btime");
	if (token > 0)
	{
		chronos.flags |= FTIME;
		converted = sscanf(token, "%*s %d", &chronos.time[BLACK]);
	}
	token = strstr(command, "winc");
	if (token > 0)
	{
		chronos.flags |= FINC;
		converted = sscanf(token, "%*s %d", &chronos.inc[WHITE]);
	}
	token = strstr(command, "binc");
	if (token > 0)
	{
		chronos.flags |= FINC;
		converted = sscanf(token, "%*s %d", &chronos.inc[BLACK]);
	}
	token = strstr(command, "movestogo");
	if (token > 0)
	{
		chronos.flags |= FMOVESTOGO;
		converted = sscanf(token, "%*s %d", &chronos.movestogo);
	}
	token = strstr(command, "depth");
	if (token > 0)
	{
		chronos.flags |= FDEPTH;
		converted = sscanf(token, "%*s %d", &chronos.depth);
	}
	token = strstr(command, "nodes");
	if (token > 0)
	{
		chronos.flags |= FNODES;
		converted = sscanf(token, "%*s %d", &chronos.nodes);
	}
	token = strstr(command, "movetime");
	if (token > 0)
	{
		chronos.flags |= FMOVETIME;
		converted = sscanf(token, "%*s %d", &chronos.movetime);
	}
	if (chronos.flags == 0)
		chronos.flags |= FINFINITE;
	search_run();
}

void UciStop() {
	chronos.timeOver = true;
}

void UciPonderhit()
{
	chronos.ponder = false;
	chronos.flags &= ~FINFINITE;
	sd.starttime = gettime();
}

void UciQuit() {
	exit(0);
}

void UciCommand(char* command)
{
	if (!strcmp(command, "uci"))
	{
		printf("id name RapCpw\n");
		printf("id author Thibor Raven\n");
		printf("option name hash type spin default 64 min 1 max 1024\n");
		printf("option name aspiration type spin default 50 min 0 max 100\n");
		printf("option name draw_opening type spin default -10 min -100 max 100\n");
		printf("option name draw_endgame type spin default 0 min -100 max 100\n");
		printf("option name UCI_Elo type spin default %d min %d max %d\n", options.eloMax,options.eloMin, options.eloMax);
		printf("option name ponder type check default %s\n", options.ponder ? "true" : "false");
		printf("uciok\n");
	}
	if (!strcmp(command, "isready"))
		printf("readyok\n");
	if (!strncmp(command, "setoption", 9))
	{
		char name[256];
		char value[256];
		if (strstr(command, "setoption name Ponder value"))
			options.ponder = (strstr(command, "value true") != 0);
		int converted = sscanf(command, "setoption name %s value %s", name, value);
		name[255] = 0;
		if (!strcmp(name, "Hash"))
		{
			int val = 64;
			converted = sscanf(value, "%d", &val);
			if (converted < 0)
			{
				tt_setsize(val << 20);
				ttpawn_setsize(val << 18);
			}
		}
		if (!strcmp(name, "aspiration"))
			converted = sscanf(value, "%d", &options.aspiration);
		if (!strcmp(name, "contempt"))
			converted = sscanf(value, "%d", &options.contempt);
		if (!strcmp(name, "UCI_Elo"))
			if (sscanf(value, "%d", &options.elo) > 0)
				setDefaultEval();
	}
	if (!strcmp(command, "ucinewgame")) {}
	if (!strncmp(command, "position", 8))
	{
		if (!strncmp(command, "position fen", 12))
			board_loadFromFen(command + 13);
		else
			board_loadFromFen(STARTFEN);
		char* moves = strstr(command, "moves");
		if (moves)
			if (!algebraic_moves(moves + 6))
				printf("wrong moves\n");
	}
	if (!strncmp(command, "go", 2))
		UciGo(command);
	if (!strcmp(command, "stop"))
		UciStop();
	if (!strcmp(command, "ponderhit"))
		UciPonderhit();
	if (!strcmp(command, "quit"))
		UciQuit();
	if (!strncmp(command, "bench",5))
		util_bench(command);
	if (!strncmp(command, "perft",5))
		util_perft(command);
	if (!strcmp(command, "eval"))
		printEval();
	if (!strcmp(command, "print"))
		PrintBoard();
}

void UciLoop() {
	while (true)
	{
		string line;
		getline(cin, line);
		UciCommand((char*)line.c_str());
	}
}