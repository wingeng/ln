/*
 * Copyright (c) 2015, Wing Eng
 * All rights reserved.
 */

#ifndef LINENOISE_PRIVATE_H
#define LINENOISE_PRIVATE_H

#include <functional>
#include <vector>

#define UNUSED __attribute__((unused))

#define S_ESC		"\x1b"
#define S_BRACKET	"["
#define S_BSPACE	"\x7f"
#define S_TAB		"\x9"
#define S_CTRL(x) (const char []) { ((x - 'A') + 1), 0 }

/*
 * The start of the  ESC [ sequence, short for
 * control sequence initiate
 */
#define CSI S_ESC S_BRACKET

/* Character handling routine */
/* Returns 0 to continue inside read loop */
/* non-zero to exit, returning status */
typedef std::function<int (int ch)> cmd_func;

void lnAddKeyHandler(const char *seq, cmd_func func);
int lnHandleKeys(int fd, int *done);
void lnPushChar(char ch);


#endif
