/*
 * Copyright (c) 2015, Wing Eng
 * All rights reserved.
 */

#ifndef LINENOISE_PRIVATE_H
#define LINENOISE_PRIVATE_H

#include <functional>
#include <vector>

#define UNUSED __attribute__((unused))

#define ESC		"\x1b"
#define BRACKET		"["
#define BSPACE		"\x7f"
#define CTRL(x) (const char []) { ((x - 'A') + 1), 0 }


/* Character handling routine */
typedef std::function<void (int ch)> cmd_func;

void ln_add_key_handler(const char *seq, cmd_func func);

void ln_handle_keys(int fd);


#endif
