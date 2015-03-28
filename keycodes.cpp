#include <map>
#include <functional>

#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "linenoise.h"

#define UNUSED __attribute__((unused))

using namespace std;

#define MAX_CHAR_BUF 32

enum KEY_ACTION {
    KEY_ANY = 0,
    CTRL_H = 8,
    CTRL_Z = 26,
    ESC = 27,   
    BACKSPACE =  127
};

enum SEQ_ENUM {
    SEQ_SELF =			1,
    SEQ_CONTROL =		2,
    SEQ_ESC =		  	3,
    SEQ_ESC_NUM =	 	4,
    SEQ_ESC_BRACKET =	        5,
    SEQ_ESC_BRACKET_NUM = 	6,
    SEQ_ESC_BRACKET_ALPHA = 	7
};

#define KC_MAKE(seq, ch) ((seq << 8) | ch)
#define KC_SEQ(kc) (kc >> 8)
#define KC_CHAR(kc) (kc & 0xFF)

/* Character handling routine */
typedef function<void (int keycode, const char *char_buf)> cmd_func;

/* Map from keycode to character handler */
typedef std::map<int, cmd_func> cmd_map_t;

static void
add_key_handler (cmd_map_t &cmd_map, int seq_type, int ch, cmd_func func)
{
    cmd_map[KC_MAKE(seq_type, ch)] = func;
}

static cmd_func
gen_cmd_func (const char *prefix)
{
    return [prefix] (int keycode, const char *char_buf UNUSED) {
	unsigned ch = KC_CHAR(keycode);
	unsigned state = KC_SEQ(keycode);
	printf("\r%10s %c : %2X:%2X\n", prefix, ch, state, ch);
    };
}

static void
esc_bracket_num_cmd (int keycode, const char *char_buf)
{
    string prefix;
    int i = 2;
    int state = KC_SEQ(keycode);
    int ch = KC_CHAR(keycode);

    prefix = "ESC [ ";
    while (isdigit(char_buf[i]))
	prefix += char_buf[i++];

    printf("\r%10s %c : %2X:%2X\n", prefix.c_str(), ch, state,  ch);
}

static void
run_key_handler (cmd_map_t cmd_map, int state, const char *char_buf, int ch)
{
    int keycode = KC_MAKE(state, ch);

    if (cmd_map.find(keycode) != cmd_map.end()) {
	cmd_map[keycode](keycode, char_buf);
    } else if (cmd_map.find(keycode & 0xFF00) != cmd_map.end()) {
	cmd_map[keycode & 0xFF00](keycode, char_buf);
    }
}

static void
next_char (int fd, char &c, char *cb, int &ci)
{
    if (ci == MAX_CHAR_BUF)
	assert(0);

    if (read(fd, &c, 1) == 1) {
	cb[ci++] = c;
    } else {
	assert(0);
    }
}

static int
keystate (int fd, cmd_map_t cmd_map)
{
    char char_buf[MAX_CHAR_BUF];
    int  cbi = 0;
    char c;

    while (1) {
	cbi = 0;
	memset(char_buf, 0, sizeof(char_buf));
	next_char(fd, c, char_buf, cbi);

	if (c == ESC) {
	    next_char(fd, c, char_buf, cbi);

            if (c == '[') {
		next_char(fd, c, char_buf, cbi);

                if (isdigit(c)) {
		    do {
			next_char(fd, c, char_buf, cbi);
		    } while (isdigit(c));

		    run_key_handler(cmd_map, SEQ_ESC_BRACKET_NUM, char_buf, c);
		    continue;

                } else {
		    run_key_handler(cmd_map, SEQ_ESC_BRACKET_ALPHA, char_buf, c);
		    continue;
                }
            }

            else if (isdigit(c)) {
		next_char(fd, c, char_buf, cbi);

		run_key_handler(cmd_map, SEQ_ESC_NUM, char_buf, c);
		continue;
            }

	    else {
		next_char(fd, c, char_buf, cbi);

		run_key_handler(cmd_map, SEQ_ESC, char_buf, c);
		continue;
	    }

	    continue;
	}

	/* One letter sequences */
	if (c == BACKSPACE)
	    c = CTRL_H;

	if (c <= CTRL_Z) {
	    run_key_handler(cmd_map, SEQ_CONTROL, char_buf, 'A' - 1 + c);
	    continue;
	} 

	run_key_handler(cmd_map, SEQ_SELF, char_buf, c);
	if (c == 'q')
	    return -1;
    }
}


int
main ()
{
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
	   "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (lnEnableRawMode(STDIN_FILENO) == -1) return -1;

    if (1) {
	cmd_map_t cmd_map;

	add_key_handler(cmd_map, SEQ_SELF,              KEY_ANY, gen_cmd_func("SELF"));
	add_key_handler(cmd_map, SEQ_CONTROL,           KEY_ANY, gen_cmd_func("CTRL"));
	add_key_handler(cmd_map, SEQ_ESC,               KEY_ANY, gen_cmd_func("ESC"));
	add_key_handler(cmd_map, SEQ_ESC_NUM,           KEY_ANY, gen_cmd_func("ESC NUM"));
	add_key_handler(cmd_map, SEQ_ESC_BRACKET,       KEY_ANY, gen_cmd_func("ESC ["));
	add_key_handler(cmd_map, SEQ_ESC_BRACKET_NUM,   KEY_ANY, esc_bracket_num_cmd);
	add_key_handler(cmd_map, SEQ_ESC_BRACKET_ALPHA, KEY_ANY, gen_cmd_func("ESC [ ALPHA"));

	keystate(STDIN_FILENO, cmd_map);
	lnDisableRawMode(STDIN_FILENO);
	return 0;
    }

    memset(quit,' ',4);
    while(1) {
        char c;
        int nread;

        nread = read(STDIN_FILENO,&c,1);
        if (nread <= 0) continue;
        memmove(quit, quit+1, sizeof(quit)-1); /* shift string to left. */
        quit[sizeof(quit)-1] = c; /* Insert current char on the right. */
        if (memcmp(quit,"quit",sizeof(quit)) == 0) break;

        printf("'%c' %02x (%d) (type quit to exit)\n",
	       isprint(c) ? c : '?', (int)c, (int)c);
        printf("\x1b[0G"); /* Go left edge manually, we are in raw mode. */
        fflush(stdout);
    }

    lnDisableRawMode(STDIN_FILENO);

    return 0;
}
