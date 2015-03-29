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

	if (ch <= *CTRL('Z')) {
	    key = "CTRL- ";
	    key += ch + 'A' - 1;
	} else {
	    key = ch;
	}
	printf("\n\r%10s %s : 0x%X", prefix, key.c_str(), ch);
	fflush(stdout);
    };
}

void
quit_me (int ch UNUSED)
{
    lnDisableRawMode(STDIN_FILENO);
    printf("\r\n");

    exit(0);
}

int
main ()
{
    printf("Key state machine codes debugging mode.\n"
	   "Press keys to see key mappings. Type 'q' at any time to exit.\n");

    if (lnEnableRawMode(STDIN_FILENO) == -1) return -1;

    ln_add_key_handler(ESC BRACKET "3~", 	gen_cmd_func("DELETE_EXT"));
    ln_add_key_handler(ESC BRACKET "5~",	gen_cmd_func("PAGE_UP"));
    ln_add_key_handler(ESC BRACKET "6~",	gen_cmd_func("PAGE_DOWN"));
    ln_add_key_handler(ESC BRACKET "A",		gen_cmd_func("HIST_PREV"));
    ln_add_key_handler(ESC BRACKET "B",		gen_cmd_func("HIST_NEXT"));
    ln_add_key_handler(ESC BRACKET "C",		gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(ESC BRACKET "D",		gen_cmd_func("MOVE_LEFT"));
    ln_add_key_handler(ESC BRACKET "F",		gen_cmd_func("END"));
    ln_add_key_handler(ESC BRACKET "H",		gen_cmd_func("HOME"));

    ln_add_key_handler(ESC "O" "F",		gen_cmd_func("END"));
    ln_add_key_handler(ESC "O" "H",		gen_cmd_func("HOME"));
    ln_add_key_handler(ESC "O" "P",		gen_cmd_func("F1"));
    ln_add_key_handler(ESC "O" "Q",		gen_cmd_func("F2"));
    ln_add_key_handler(ESC "b",			gen_cmd_func("MOVE_LEFT_WORD"));
    ln_add_key_handler(ESC "d",			gen_cmd_func("DELETE_RIGHT_WORD"));
    ln_add_key_handler(ESC "f",			gen_cmd_func("MOVE_RIGHT_WORD"));
    ln_add_key_handler(ESC "h",			gen_cmd_func("DELETE_LEFT_WORD"));

    ln_add_key_handler(CTRL('A'),	     	gen_cmd_func("MOVE_BEGIN_LINE"));
    ln_add_key_handler(CTRL('B'),		gen_cmd_func("MOVE_LEFT"));
    ln_add_key_handler(CTRL('C'),		gen_cmd_func("CANCEL"));
    ln_add_key_handler(CTRL('D'),		gen_cmd_func("DELETE"));
    ln_add_key_handler(CTRL('E'),		gen_cmd_func("MOVE_END_LINE"));
    ln_add_key_handler(CTRL('F'),		gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(CTRL('F'),		gen_cmd_func("MOVE_RIGHT"));
    ln_add_key_handler(CTRL('H'),		gen_cmd_func("BACKSPACE"));
    ln_add_key_handler(BSPACE,     		gen_cmd_func("BACKSPACE"));
    ln_add_key_handler(CTRL('L'),		gen_cmd_func("CLEAR_SCREEN"));
    ln_add_key_handler(CTRL('M'),		gen_cmd_func("ENTER"));
    ln_add_key_handler(CTRL('N'),		gen_cmd_func("HIST_NEXT"));
    ln_add_key_handler(CTRL('P'),		gen_cmd_func("HIST_PREV"));
    ln_add_key_handler(CTRL('T'),		gen_cmd_func("SWAP"));
    ln_add_key_handler(CTRL('U'),		gen_cmd_func("DELETE_LINE"));
    ln_add_key_handler(CTRL('W'),		gen_cmd_func("DELETE_PREV_WORD"));

    ln_add_key_handler("q",			quit_me);
    ln_add_key_handler("*",			gen_cmd_func("SELF"));

    ln_handle_keys(STDIN_FILENO);

    lnDisableRawMode(STDIN_FILENO);
    printf("\r\n");

    return 0;
}

