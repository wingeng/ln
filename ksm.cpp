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
gen_cmd_func (const char *prefix)
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

    ln_add_key_handler(S_ESC S_BRACKET "3~", 	gen_cmd_func("DELETE_EXT"));
    ln_add_key_handler(S_ESC S_BRACKET "5~",	gen_cmd_func("PAGE_UP"));
    ln_add_key_handler(S_ESC S_BRACKET "6~",	gen_cmd_func("PAGE_DOWN"));
    ln_add_key_handler(S_ESC S_BRACKET "A", 	gen_cmd_func("HIST_PREV"));
    ln_add_key_handler(S_ESC S_BRACKET "B", 	gen_cmd_func("HIST_NEXT"));
    ln_add_key_handler(S_ESC S_BRACKET "C", 	gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(S_ESC S_BRACKET "D", 	gen_cmd_func("MOVE_LEFT"));
    ln_add_key_handler(S_ESC S_BRACKET "F", 	gen_cmd_func("END"));
    ln_add_key_handler(S_ESC S_BRACKET "H", 	gen_cmd_func("HOME"));

    ln_add_key_handler(S_ESC "O" "F",		gen_cmd_func("END"));
    ln_add_key_handler(S_ESC "O" "H",		gen_cmd_func("HOME"));
    ln_add_key_handler(S_ESC "O" "P",		gen_cmd_func("F1"));
    ln_add_key_handler(S_ESC "O" "Q",		gen_cmd_func("F2"));
    ln_add_key_handler(S_ESC "b", 		gen_cmd_func("MOVE_LEFT_WORD"));
    ln_add_key_handler(S_ESC "d", 		gen_cmd_func("DELETE_RIGHT_WORD"));
    ln_add_key_handler(S_ESC "f", 		gen_cmd_func("MOVE_RIGHT_WORD"));
    ln_add_key_handler(S_ESC "h", 		gen_cmd_func("DELETE_LEFT_WORD"));
    ln_add_key_handler(S_ESC S_BSPACE, 		gen_cmd_func("DELETE_LEFT_WORD"));

    ln_add_key_handler(S_CTRL('A'),	     	gen_cmd_func("MOVE_BEGIN_LINE"));
    ln_add_key_handler(S_CTRL('B'),		gen_cmd_func("MOVE_LEFT"));
    ln_add_key_handler(S_CTRL('C'),		gen_cmd_func("CANCEL"));
    ln_add_key_handler(S_CTRL('D'),		gen_cmd_func("DELETE"));
    ln_add_key_handler(S_CTRL('E'),		gen_cmd_func("MOVE_END_LINE"));
    ln_add_key_handler(S_CTRL('F'),		gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(S_CTRL('F'),		gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(S_CTRL('H'),		gen_cmd_func("BACKSPACE"));
    ln_add_key_handler(S_BSPACE,     		gen_cmd_func("BACKSPACE"));
    ln_add_key_handler(S_CTRL('L'),		gen_cmd_func("CLEAR_SCREEN"));
    ln_add_key_handler(S_CTRL('M'),		gen_cmd_func("ENTER"));
    ln_add_key_handler(S_CTRL('N'),		gen_cmd_func("HIST_NEXT"));
    ln_add_key_handler(S_CTRL('P'),		gen_cmd_func("HIST_PREV"));
    ln_add_key_handler(S_CTRL('T'),		gen_cmd_func("SWAP"));
    ln_add_key_handler(S_CTRL('U'),		gen_cmd_func("DELETE_LINE"));
    ln_add_key_handler(S_CTRL('W'),		gen_cmd_func("DELETE_PREV_WORD"));

    ln_add_key_handler("q",			[&done] (char ch UNUSED) { done = 1; return 0;});
    ln_add_key_handler("*",			gen_cmd_func("SELF"));

    ln_handle_keys(STDIN_FILENO, &done);

    lnDisableRawMode(STDIN_FILENO);
    printf("\r\n");

    return 0;
}

