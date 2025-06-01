#include "program.h"

using namespace std;

bool pipe;
HANDLE hstdin;

int InputInit()
{
	unsigned long dw;
	hstdin = GetStdHandle(STD_INPUT_HANDLE);
	pipe = !GetConsoleMode(hstdin, &dw);
	if (!pipe)
	{
		SetConsoleMode(hstdin, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
		FlushConsoleInputBuffer(hstdin);
	}
	else
	{
		setvbuf(stdin, NULL, _IONBF, 0);
		setvbuf(stdout, NULL, _IONBF, 0);
	}
	return 0;
}

bool input() {
	unsigned long dw = 0;
	if (pipe)
		PeekNamedPipe(hstdin, 0, 0, 0, &dw, 0);
	else
		GetNumberOfConsoleInputEvents(hstdin, &dw);
	return dw > 1;
}

char* getsafe(char* buffer, int count)
{
	char* result = buffer, * np;
	if ((buffer == NULL) || (count < 1))
		result = NULL;
	else if (count == 1)
		*result = '\0';
	else if ((result = fgets(buffer, count, stdin)) != NULL)
		if (np = strchr(buffer, '\n'))
			*np = '\0';
	return result;
}

bool GetInput(string &s)
{
	char command[5000];
	if (!input())
		return false;
	getsafe(command, sizeof command);
	s= string(command);
	return true;
}