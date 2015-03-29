/*
 *
 * Command handler for maping characters from stdin to command
 * handler.  Use ln_add_key_handler() to build the tree of key sequences
 * that map to function calls.
 *
 * Example: to call the forward_char function for the escape sequence
 *
 *
 * Copyright (c) 2015, Wing Eng
 * All rights reserved.
 */
#include <deque>
#include <assert.h>

#include "linenoise.h"
#include "linenoise_private.h"

using namespace std;

class key_node_c {
public:
    key_node_c (int ch) : kn_ch(ch) {};

public:
    int kn_ch;
    cmd_func kn_func;

    vector<key_node_c *> kn_children;
};

typedef vector<key_node_c *> cmd_vec_t;

static cmd_vec_t cmd_map;
static deque<char> char_stack;

static key_node_c *
find_key_node (cmd_vec_t &cmd_map, int ch, int match_any = 1)
{
    for (auto kn : cmd_map) {
	if (kn->kn_ch == ch)
	    return kn;
	if (kn->kn_ch == '*' && match_any)
	    return kn;
    }
    return NULL;
}

static void
next_char (int fd, char &c)
{
    if (char_stack.size()) {
	c = char_stack.front();
	char_stack.pop_front();
	return;
    }

    if (read(fd, &c, 1) != 1) {
	assert(0);
    }
}

/*
 * Push a char back into the read loop, use this char instead
 * of stdin, used for complete line where the char wasn't consumed
 */
void
ln_push_char (char c)
{
    char_stack.push_back(c);
}

static key_node_c *
ln_get_keys (cmd_vec_t &cmd_map, int fd, char &ch)
{
    next_char(fd, ch);

    auto kn = find_key_node(cmd_map, ch);
    if (kn) {
	if (kn->kn_children.size()) {
	    return ln_get_keys(kn->kn_children, fd, ch);
	} else {
	    if (kn->kn_func)
		return kn;
	}
    }

    return NULL;
}

static void
add_key_handler (cmd_vec_t &cmap, const char *seq, cmd_func func)
{
    key_node_c *kn;

    if ((kn = find_key_node(cmap, *seq, 0)) == NULL) {
	kn = new key_node_c(*seq);

	cmap.push_back(kn);
    }

    kn->kn_func = func;
    if (seq[1] == '\0')
	return;

    add_key_handler(kn->kn_children, seq + 1, func);
}

void
ln_add_key_handler (const char *seq, cmd_func func)
{
    add_key_handler(cmd_map, seq, func);
}

int
ln_handle_keys (int fd, int *done)
{
    char ch;
    int ret = 00;

    while (!*done) {
	auto kn = ln_get_keys(cmd_map, fd, ch);
	if (kn && kn->kn_func) {
	    ret = kn->kn_func(ch);
	}
    }
    return ret;
}
