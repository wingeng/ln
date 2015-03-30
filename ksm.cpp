/*
 * Demostrates the key-state-machine code.  Inserts function that map to
 * key sequences.
 *
 * Copyright (c) 2015, Wing Eng
 * All rights reserved.
 */
#include <string>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "linenoise.h"
#include "linenoise_private.h"

using namespace std;

/*
 * Generate a function which handles a keypress
 * Capture the 'prefix' in closure to output the type
 * of key press.
 */
static cmd_func
genCmdFunc (const char *prefix)
{
    return [prefix] (int ch) {
	string key;

	if (ch <= *S_CTRL('Z')) {
	    key = "S_CTRL- ";
	    key += ch + 'A' - 1;
	} else {
	    key = ch;
	}
	printf("\n\r%10s %s : 0x%X", prefix, key.c_str(), ch);
	fflush(stdout);

	return 0;
    };
}

int
quit_me (int ch UNUSED)
{
    return 1;
}

int
main ()
{
    int done = 0;

    printf("Key state machine codes debugging mode.\n"
	   "Press keys to see key mappings. Type 'q' at any time to exit.\n");

    if (lnEnableRawMode(STDIN_FILENO) == -1) return -1;

    lnAddKeyHandler(S_ESC S_BRACKET "3~", 	genCmdFunc("DELETE_EXT"));
    lnAddKeyHandler(S_ESC S_BRACKET "5~",	genCmdFunc("PAGE_UP"));
    lnAddKeyHandler(S_ESC S_BRACKET "6~",	genCmdFunc("PAGE_DOWN"));
    lnAddKeyHandler(S_ESC S_BRACKET "A", 	genCmdFunc("HIST_PREV"));
    lnAddKeyHandler(S_ESC S_BRACKET "B", 	genCmdFunc("HIST_NEXT"));
    lnAddKeyHandler(S_ESC S_BRACKET "C", 	genCmdFunc("MOVE_RIGHT"));
    lnAddKeyHandler(S_ESC S_BRACKET "D", 	genCmdFunc("MOVE_LEFT"));
    lnAddKeyHandler(S_ESC S_BRACKET "F", 	genCmdFunc("END"));
    lnAddKeyHandler(S_ESC S_BRACKET "H", 	genCmdFunc("HOME"));

    lnAddKeyHandler(S_ESC "O" "F",		genCmdFunc("END"));
    lnAddKeyHandler(S_ESC "O" "H",		genCmdFunc("HOME"));
    lnAddKeyHandler(S_ESC "O" "P",		genCmdFunc("F1"));
    lnAddKeyHandler(S_ESC "O" "Q",		genCmdFunc("F2"));
    lnAddKeyHandler(S_ESC "b",			genCmdFunc("MOVE_LEFT_WORD"));
    lnAddKeyHandler(S_ESC "d",			genCmdFunc("DELETE_RIGHT_WORD"));
    lnAddKeyHandler(S_ESC "f",			genCmdFunc("MOVE_RIGHT_WORD"));
    lnAddKeyHandler(S_ESC "h",			genCmdFunc("DELETE_LEFT_WORD"));
    lnAddKeyHandler(S_ESC S_BSPACE, 		genCmdFunc("DELETE_LEFT_WORD"));

    lnAddKeyHandler(S_CTRL('A'),	     	genCmdFunc("MOVE_BEGIN_LINE"));
    lnAddKeyHandler(S_CTRL('B'),		genCmdFunc("MOVE_LEFT"));
    lnAddKeyHandler(S_CTRL('C'),		genCmdFunc("CANCEL"));
    lnAddKeyHandler(S_CTRL('D'),		genCmdFunc("DELETE"));
    lnAddKeyHandler(S_CTRL('E'),		genCmdFunc("MOVE_END_LINE"));
    lnAddKeyHandler(S_CTRL('F'),		genCmdFunc("MOVE_RIGHT"));
    lnAddKeyHandler(S_CTRL('F'),		genCmdFunc("MOVE_RIGHT"));
    lnAddKeyHandler(S_CTRL('H'),		genCmdFunc("BACKSPACE"));
    lnAddKeyHandler(S_BSPACE,     		genCmdFunc("BACKSPACE"));
    lnAddKeyHandler(S_CTRL('L'),		genCmdFunc("CLEAR_SCREEN"));
    lnAddKeyHandler(S_CTRL('M'),		genCmdFunc("ENTER"));
    lnAddKeyHandler(S_CTRL('N'),		genCmdFunc("HIST_NEXT"));
    lnAddKeyHandler(S_CTRL('P'),		genCmdFunc("HIST_PREV"));
    lnAddKeyHandler(S_CTRL('T'),		genCmdFunc("SWAP"));
    lnAddKeyHandler(S_CTRL('U'),		genCmdFunc("DELETE_LINE"));
    lnAddKeyHandler(S_CTRL('W'),		genCmdFunc("DELETE_PREV_WORD"));

    lnAddKeyHandler("q",			[&done] (char ch UNUSED) { done = 1; return 0;});
    lnAddKeyHandler("*",			genCmdFunc("SELF"));

    lnHandleKeys(STDIN_FILENO, &done);

    lnDisableRawMode(STDIN_FILENO);
    printf("\r\n");

    return 0;
}

