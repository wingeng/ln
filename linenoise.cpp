/* linenoise.cpp -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 * 
 * Original License in license.txt file
 */
#include <functional>
#include <string>
#include <termios.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "string_fmt.h"
#include "linenoise.h"
#include "linenoise_private.h"

using namespace std;

class lnCompletion {
public:
    lnCompletion(const char *tok, const char *help) :
	lnc_token(tok), lnc_help(help) {};
    lnCompletion(const std::string &tok, const std::string &help) :
	lnc_token(tok), lnc_help(help) {};

    std::string lnc_token;
    std::string lnc_help;
};

#define ESC	27
#define TAB	9


#define LN_DEFAULT_HISTORY_MAX_LEN 100
#define LN_MAX_LINE 4096
static const char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static lnCompletionFunc cppCompletionCallback;

static struct termios orig_termios; /* In order to restore at exit.*/
static int rawmode = 0; /* For atexit() function to check if restore is needed*/
static int atexit_registered = 0; /* Register atexit just 1 time. */
static int history_max_len = LN_DEFAULT_HISTORY_MAX_LEN;

static std::vector<std::string> history;

/* The linenoiseState structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
struct linenoiseState {
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */

    int history_index;
    int edit_done;      /* set non-zero when done with editing line */
};

static void lnAtExit(void);
int lnHistoryAdd(const char *line);
static void refreshLine(struct linenoiseState *ls);
static void refreshSingleLine(struct linenoiseState *ls);

/* Debugging macro. */
#if 1
FILE *lndebug_fp = NULL;
#define wndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt", "a"); \
	} \
        fprintf(lndebug_fp,  __VA_ARGS__);	\
	fflush(lndebug_fp);				\
    } while (0)
#else
#define wndebug(fmt, ...)
#endif

/* ======================= Low level terminal handling ====================== */

/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
static int
isUnsupportedTerm (void)
{
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return 0;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term, unsupported_term[j])) return 1;
    return 0;
}

/* Raw mode: 1960 magic shit. */
int
lnEnableRawMode (int fd)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(lnAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd, &orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

 fatal:
    errno = ENOTTY;
    return -1;
}

void
lnDisableRawMode (int fd)
{
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd, TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int
getCursorPosition (int ifd, int ofd)
{
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd, buf + i, 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf + 2, "%d;%d", &rows, &cols) != 2) return -1;
    return cols;
}

static void
get_col_row (unsigned int &cols, unsigned int &rows)
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
	/* handle this case later */
	assert(0);
    }

    cols = ws.ws_col;
    rows = ws.ws_row;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int
getColumns (int ifd, int ofd)
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = getCursorPosition(ifd, ofd);
        if (start == -1) goto failed;

        /* Go to right margin and get position. */
        if (write(ofd, "\x1b[999C", 6) != 6) goto failed;
        cols = getCursorPosition(ifd, ofd);
        if (cols == -1) goto failed;

        /* Restore position. */
        if (cols > start) {
	    string_fmt_c s;

	    s.format("\x1b[%dD", cols-start);
            if (write(ofd, s.c_str(), s.size()) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }

 failed:
    return 80;
}

/* Clear the screen. Used to handle ctrl+l */
void
lnClearScreen (linenoiseState *ls UNUSED)
{
    if (write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void
lnBeep (void)
{
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

static string
longestMatch (std::vector<lnCompletion> *lc)
{
    const char *s;
    string longest;

    if (lc->size() <= 0) return "";
    
    longest = (*lc)[0].lnc_token;
    for (auto comp : (*lc)) {
	s = comp.lnc_token.c_str();
	while (longest.size() &&
	       longest.compare(0, longest.size(), s, longest.size())) {
	    longest = longest.substr(0, longest.size() - 1);
	}
	if (longest.size() == 0)
	    break;
    }
	
    return longest;
}

static void
helpLine (struct linenoiseState *ls)
{
    std::vector<lnCompletion> lc;
    unsigned int max_cols, max_rows;
    
    get_col_row(max_cols, max_rows);
    cppCompletionCallback(ls->buf, &lc);

    if (lc.size() == 0) {
	printf("\r\n *no-match*");
    } else {
	// maximum rows to display is the screen rows less
	// 1 for prompt and 1 for the '... more ...' msg
	if (max_rows > 1) max_rows -= 2;

	unsigned int i = 0;
	for (auto comp : lc) {
	    if (i >= max_rows) break;
	    auto tok = comp.lnc_token.c_str();
	    auto help = comp.lnc_help.c_str();
	    printf("\r\n %-20s %s", tok, help);
	    wndebug("help %d - %s\n", i, tok);
	    i++;
        }
	if (lc.size() >= max_rows) {
	    printf("\r\n      ... more ...");
	    wndebug("need more\n");
	}
    }
    printf("\n\r");
    fflush(stdout);

    refreshSingleLine(ls);
    fflush(stdout);
}

/* This is an helper function for lnEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 * 
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
static void
completeLine (struct linenoiseState *ls)
{
    std::vector<lnCompletion> lc;
    int nread, nwritten;
    char c = 0;

    if (!cppCompletionCallback) return;

    cppCompletionCallback(ls->buf, &lc);
    if (lc.size() == 0) {
        lnBeep();
    } else {
        size_t stop = 0;
	auto longest = longestMatch(&lc);

	wndebug("longest: %s\n", longest.c_str());

	nwritten = snprintf(ls->buf, ls->buflen, "%s", longest.c_str());
	ls->len = ls->pos = nwritten;
	refreshLine(ls);

        while (!stop) {
            nread = read(ls->ifd, &c, 1);
            if (nread <= 0) {
		assert(0);
            }

            switch (c) {
	    case TAB:
		lnBeep();
	    case '?':
		helpLine(ls);
		break;
	    default:
		stop = 1;
		break;
            }
        }
    }

    ln_push_char(c);
}

/* Register a callback function to be called for tab-completion. */
void
lnSetCompletionCallback (lnCompletionFunc fn)
{
    cppCompletionCallback = fn;
}


/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void
lnAddCompletion (lnCompletionVec *lc, const char *str, const char *help)
{
    lc->push_back(lnCompletion(str, help));
}


/* =========================== Line editing ================================= */


/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void
refreshSingleLine (struct linenoiseState *ls)
{
    size_t plen = strlen(ls->prompt);
    int fd = ls->ofd;
    char *buf = ls->buf;
    size_t len = ls->len;
    size_t pos = ls->pos;
    string_fmt_c ab;
    
    while ((plen + pos) >= ls->cols) {
        buf++;
        len--;
        pos--;
    }
    while ((plen + len) > ls->cols) {
        len--;
    }

    /* Cursor to left edge */
    ab = "\x1b[0G";

    /* Write the prompt and the current buffer content */
    ab += ls->prompt;
    ab += buf;

    /* Erase to right */
    ab += "\x1b[0K";

    /* Move cursor to original position. */
    ab.append("\x1b[0G\x1b[%dC", (int) (pos + plen));

    if (write(fd, ab.c_str(), ab.size()) == -1) {} /* Can't recover from write error. */
}

/* Calls the two low level functions refreshSingleLine() or
 * refreshMultiLine() according to the selected mode. */
static void
refreshLine(struct linenoiseState *ls)
{
    refreshSingleLine(ls);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
int
lnEditInsert (struct linenoiseState *ls, char c)
{
    if (c <= ESC) return 0;

    if (ls->len < ls->buflen) {
        if (ls->len == ls->pos) {
            ls->buf[ls->pos] = c;
            ls->pos++;
            ls->len++;
            ls->buf[ls->len] = '\0';
            if (ls->plen+ls->len < ls->cols) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (write(ls->ofd, &c, 1) == -1) return -1;
            }
        } else {
            memmove(ls->buf+ls->pos + 1, ls->buf + ls->pos, ls->len-ls->pos);
            ls->buf[ls->pos] = c;
            ls->len++;
            ls->pos++;
            ls->buf[ls->len] = '\0';
        }
    }
    refreshLine(ls);
    return 0;
}

/* Move cursor on the left. */
void
lnEditMoveLeft (struct linenoiseState *ls)
{
    if (ls->pos > 0) {
        ls->pos--;
    }
}

/* Move cursor on the right. */
void
lnEditMoveRight (struct linenoiseState *ls)
{
    if (ls->pos != ls->len) {
        ls->pos++;
    }
}

static int
isWordSep (int ch)
{
    return !isalnum(ch);
}

static void
lnMoveWord (struct linenoiseState *ls, int dir)
{
    if ((int) ls->pos + dir < 0) return;
    if ((int) ls->pos + dir >= (int) ls->len) return;

    ls->pos += dir;
    while (ls->pos > 0 && ls->pos != ls->len && isWordSep(ls->buf[ls->pos]))
	ls->pos += dir;

    while (ls->pos > 0 && ls->pos != ls->len && !isWordSep(ls->buf[ls->pos-1]))
	ls->pos += dir;
}

static void
lnEditMoveLeftWord (struct linenoiseState *ls)
{
    if (ls->pos == 0) return;
    lnMoveWord(ls, -1);
}

static void
lnEditMoveRightWord (struct linenoiseState *ls)
{
    if (ls->pos == ls->len) return;
    lnMoveWord(ls, 1);
}

static void
lnEditMoveHome (struct linenoiseState *ls)
{
    if (ls->pos == 0) return;
    ls->pos = 0;
}

static void
lnEditMoveEnd (struct linenoiseState *ls)
{
    if (ls->pos == ls->len) return;
    ls->pos = ls->len;
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
static void
editHistoryNext (struct linenoiseState *ls, int dir)
{
    int *history_index = &ls->history_index;

    if (history.size() > 1) {
	auto history_len = (int) history.size();

        /* Update the current history entry before to
         * overwrite it with the next one. */
        history[history_len - 1 - *history_index] = ls->buf;
	
        /* Show the new entry */
        *history_index += dir;
        if (*history_index < 0) {
            *history_index = 0;
            return;
        } else if (*history_index >= history_len) {
            *history_index = history_len - 1;
            return;
        }
        strncpy(ls->buf, history[history_len - 1 - *history_index].c_str(), ls->buflen);
        ls->buf[ls->buflen - 1] = '\0';
        ls->len = ls->pos = strlen(ls->buf);
    }
}

static void
lnEditHistoryPrev (linenoiseState *ls)
{
    editHistoryNext(ls, 1);
}

static void
lnEditHistoryNext (linenoiseState *ls)
{
    editHistoryNext(ls, -1);
}


/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
void
lnEditDelete (struct linenoiseState *ls)
{
    if (ls->len > 0 && ls->pos < ls->len) {
        memmove(ls->buf+ls->pos, ls->buf+ls->pos+1, ls->len-ls->pos-1);
        ls->len--;
        ls->buf[ls->len] = '\0';
    }
}

void
lnEditBackspace (struct linenoiseState *ls)
{
    if (ls->pos > 0 && ls->len > 0) {
        memmove(ls->buf+ls->pos-1, ls->buf+ls->pos, ls->len-ls->pos);
        ls->pos--;
        ls->len--;
        ls->buf[ls->len] = '\0';
    }
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. */
void
lnEditDeletePrevWord (struct linenoiseState *ls)
{
    size_t old_pos = ls->pos;
    size_t diff;

    if (ls->pos == 0) return;

    lnMoveWord(ls, -1);
    diff = old_pos - ls->pos;
    memmove(ls->buf+ls->pos, ls->buf+old_pos, ls->len-old_pos+1);
    ls->len -= diff;
}

/* Delete the next word, maintaining the cursor at the start of the
 * current word. */
void
lnEditDeleteNextWord (struct linenoiseState *ls)
{
    size_t old_pos = ls->pos;
    size_t diff;

    if (ls->pos == ls->len) return;

    lnMoveWord(ls, 1);
    diff = ls->pos - old_pos;
    memmove(ls->buf + old_pos, ls->buf + ls->pos, ls->len - ls->pos+1);
    ls->len -= diff;
    ls->pos = old_pos;
}

static void
lnEditSwap (linenoiseState *ls)
{
    if (ls->pos > 0 && ls->pos < ls->len) {
	int aux = ls->buf[ls->pos - 1];
	ls->buf[ls->pos - 1] = ls->buf[ls->pos];
	ls->buf[ls->pos] = aux;
	if (ls->pos != ls->len-1) (ls->pos)++;
    }
}

static void
lnEditDeleteLine (linenoiseState *ls)
{
    ls->buf = '\0';
    ls->pos = ls->len = 0;
}

static void
lnEditDeleteToEOL (linenoiseState *ls)
{
    ls->buf[ls->pos] = '\0';
    ls->len = ls->pos;
}

typedef void (ln_func_t)(linenoiseState *);

static cmd_func
ln_cmd (linenoiseState *ls, ln_func_t func)
{
    return [ls, func] (int ch UNUSED) {
	func(ls);
	refreshLine(ls);
	return 0;
    };
}

/* This function is the core of the line editing capability of linenoise.
 * It expects 'fd' to be already in "raw mode" so that every key pressed
 * will be returned ASAP to read().
 *
 * The resulting string is put into 'buf' when the user type enter, or
 * when ctrl+d is typed.
 *
 * The function returns the length of the current buffer. */
static int
lnEdit (int stdin_fd, int stdout_fd,
	char *buf, size_t buflen, const char *prompt)
{
    struct linenoiseState l;
    struct linenoiseState *ls = &l;

    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l.ifd = stdin_fd;
    l.ofd = stdout_fd;
    l.buf = buf;
    l.buflen = buflen;
    l.prompt = prompt;
    l.plen = strlen(prompt);
    l.oldpos = l.pos = 0;
    l.len = 0;
    l.cols = getColumns(stdin_fd, stdout_fd);
    l.edit_done = 0;
    l.history_index = 0;

    /* Buffer starts empty. */
    l.buf[0] = '\0';
    l.buflen--; /* Make sure there is always space for the nulterm */

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");
    
    if (write(l.ofd, prompt, l.plen) == -1) return -1;

    ln_add_key_handler(S_CTRL('M'), [ls] (int ch UNUSED) {
	    history.pop_back();
	    refreshLine(ls);
	    ls->edit_done = 1;
            return (int) ls->len;
	});

    ln_add_key_handler(S_CTRL('C'), [ls] (int ch UNUSED) {
            errno = EAGAIN;
	    ls->edit_done = 1;
            return -1;
	});

    ln_add_key_handler(S_TAB, 	ln_cmd(ls, completeLine));
    ln_add_key_handler("?", 	ln_cmd(ls, helpLine));
    ln_add_key_handler(S_CTRL('H'), ln_cmd(ls, lnEditBackspace));
    ln_add_key_handler(S_BSPACE,    ln_cmd(ls, lnEditBackspace));
    ln_add_key_handler(S_CTRL('T'), ln_cmd(ls, lnEditSwap));
    ln_add_key_handler(S_CTRL('B'), ln_cmd(ls, lnEditMoveLeft));
    ln_add_key_handler(S_CTRL('F'), ln_cmd(ls, lnEditMoveRight));
    ln_add_key_handler(S_CTRL('P'), ln_cmd(ls, lnEditHistoryPrev));
    ln_add_key_handler(S_CTRL('N'), ln_cmd(ls, lnEditHistoryNext));
    ln_add_key_handler(S_CTRL('U'), ln_cmd(ls, lnEditDeleteLine));
    ln_add_key_handler(S_CTRL('K'), ln_cmd(ls, lnEditDeleteToEOL));
    ln_add_key_handler(S_CTRL('A'), ln_cmd(ls, lnEditMoveHome));
    ln_add_key_handler(S_CTRL('E'), ln_cmd(ls, lnEditMoveEnd));
    ln_add_key_handler(S_CTRL('L'), ln_cmd(ls, lnClearScreen));
    ln_add_key_handler(S_CTRL('W'), ln_cmd(ls, lnEditDeletePrevWord));

    ln_add_key_handler(S_CTRL('D'), [ls] (int ch UNUSED) {
            if (ls->len > 0) {
                lnEditDelete(ls);
		refreshLine(ls);
		return 0;
            } else {
		history.pop_back();
		ls->edit_done = 1;
                return -1;
            }
	});

    ln_add_key_handler(S_ESC S_BRACKET "3~", ln_cmd(ls, lnEditDelete));

    ln_add_key_handler(S_ESC S_BRACKET "A", ln_cmd(ls, lnEditHistoryPrev));
    ln_add_key_handler(S_ESC S_BRACKET "B", ln_cmd(ls, lnEditHistoryNext));
    ln_add_key_handler(S_ESC S_BRACKET "C", ln_cmd(ls, lnEditMoveRight));
    ln_add_key_handler(S_ESC S_BRACKET "D", ln_cmd(ls, lnEditMoveLeft));
    ln_add_key_handler(S_ESC S_BRACKET "H", ln_cmd(ls, lnEditMoveHome));
    ln_add_key_handler(S_ESC S_BRACKET "F", ln_cmd(ls, lnEditMoveEnd));

    ln_add_key_handler(S_ESC S_ESC S_BRACKET "C", ln_cmd(ls, lnEditMoveRightWord));
    ln_add_key_handler(S_ESC S_ESC S_BRACKET "D", ln_cmd(ls, lnEditMoveLeftWord));

    ln_add_key_handler(S_ESC "O" "H", ln_cmd(ls, lnEditMoveHome));
    ln_add_key_handler(S_ESC "O" "F", ln_cmd(ls, lnEditMoveEnd));

    ln_add_key_handler(S_ESC "b", ln_cmd(ls, lnEditMoveLeftWord));
    ln_add_key_handler(S_ESC "f", ln_cmd(ls, lnEditMoveRightWord));
    ln_add_key_handler(S_ESC "h", ln_cmd(ls, lnEditDeletePrevWord));
    ln_add_key_handler(S_ESC "d", ln_cmd(ls, lnEditDeleteNextWord));
    ln_add_key_handler(S_ESC S_BSPACE, ln_cmd(ls, lnEditDeletePrevWord));

    /*
     * This has to be the last handler, to take care of all 'other'
     * keys
     */
    ln_add_key_handler("*", [ls] (int c) {
            if (lnEditInsert(ls, c)) {
		ls->edit_done = 1;
		return -1;
	    }
	    return 0;
	});

    return ln_handle_keys(stdin_fd, &ls->edit_done);
}

/* This function calls the line editing function lnEdit() using
 * the STDIN file descriptor set in raw mode. */
static int
lnRaw (char *buf, size_t buflen, const char *prompt)
{
    int count;

    if (buflen == 0) {
        errno = EINVAL;
        return -1;
    }

    if (lnEnableRawMode(STDIN_FILENO) == -1) return -1;
    count = lnEdit(STDIN_FILENO, STDOUT_FILENO, buf, buflen, prompt);
    lnDisableRawMode(STDIN_FILENO);
    printf("\n");

    return count;
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions. */
char *
linenoise (const char *prompt)
{
    char buf[LN_MAX_LINE];
    int count;

    if (isUnsupportedTerm() || !isatty(STDIN_FILENO)) {
        size_t len;

	if (isatty(STDIN_FILENO)) {
	    printf("%s", prompt);
	    fflush(stdout);
	}

        if (fgets(buf, LN_MAX_LINE, stdin) == NULL) return NULL;
        len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
    } else {
        count = lnRaw(buf, LN_MAX_LINE, prompt);
        if (count == -1) return NULL;
    }

    return strdup(buf);
}

/* ================================ History ================================= */

/* At exit we'll try to fix the terminal to the initial conditions. */
static void
lnAtExit (void)
{
    lnDisableRawMode(STDIN_FILENO);
}

/* This is the API call to add a new entry in the linenoise history. */

int
linenoiseHistoryAdd (const char *line)
{
    if (history_max_len == 0) return 0;

    /* Don't add duplicated lines. */
    auto history_len = (int) history.size();
    if (history.size() && history[history_len-1] == line)
	return 0;

    if (history_len == history_max_len) {
        history.erase(history.begin());
    }

    history.push_back(line);
    return 1;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int
lnHistorySetMaxLen (int len)
{
    if (len < 1) return 0;

    while ((int) history.size() >= len) {
	history.erase(history.begin());
    }

    history_max_len = len;

    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int
linenoiseHistorySave (const char *filename)
{
    FILE *fp = fopen(filename, "w");
    
    if (fp == NULL) return -1;
    for (auto s : history) 
        fprintf(fp, "%s\n", s.c_str());
    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int
linenoiseHistoryLoad (const char *filename)
{
    FILE *fp = fopen(filename, "r");
    char buf[LN_MAX_LINE];
    
    if (fp == NULL) return -1;

    while (fgets(buf, LN_MAX_LINE, fp) != NULL) {
        char *p;
        
        p = strchr(buf, '\r');
        if (!p) p = strchr(buf, '\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}
