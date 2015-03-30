/* 
 * Copyright (c) 2015, Wing Eng
 * All rights reserved.
 */
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linenoise.h"

#define UNUSED __attribute__((unused))

typedef void (*command_action_t)(const char *line);
typedef struct command_s {
    const char *c_token;
    const char *c_help;
    command_action_t c_action;
} command_t;

/*
 * Syntax for declaring a function pointer
 * between the '<>' enter the return type and function
 * arguments.
 */
typedef std::function<void (command_t &)> complete_cb;

static void help_cmd_action(const char *);
static void quit_cmd_action(const char *);
static void generic_cmd_action(const char *);

command_t cmds[] = {
    { "hello", "help for hello", help_cmd_action },
    { "helo",  "help for helo",  help_cmd_action },
    { "joe",   "help for joe",   generic_cmd_action },
    { "james", "help for james", generic_cmd_action },
    { "quit",  "quit from test", quit_cmd_action },

    { "blah0", "help for blah0", generic_cmd_action },
    { "blah1", "help for blah1", generic_cmd_action },
    { "blah2", "help for blah2", generic_cmd_action },
    { "blah3", "help for blah3", generic_cmd_action },
    { "blah4", "help for blah4", generic_cmd_action },
    { "blah5", "help for blah5", generic_cmd_action },
    { "blah6", "help for blah6", generic_cmd_action },
    { "blah7", "help for blah7", generic_cmd_action },
    { "blah8", "help for blah8", generic_cmd_action },
    { "blah9", "help for blah9", generic_cmd_action },
};

static void
help_cmd_action (const char *line)
{
    printf("help for '%s'\n", line);
}

static void
generic_cmd_action (const char *line)
{
    printf("generic for '%s'\n", line);
}

static void
quit_cmd_action (const char *line UNUSED)
{
    exit(0);
}

static int
match_prefix (const char *whole, const char *partial) 
{
    return partial == NULL || strlen(partial) == 0 ||
	strncmp(whole, partial, strlen(partial)) == 0;
}

static void
command_match (const char *buf, complete_cb cb)
{
    for (auto cmd : cmds) {
	if (match_prefix(cmd.c_token, buf))
	    cb(cmd);
    }
}

void
call_command (const char *buf)
{
    int n_commands = 0;
    command_action_t found_action = NULL;
    
    command_match(buf, [&] (command_t &cmd) {
	    found_action = cmd.c_action;
	    n_commands++;
	});

    switch (n_commands) {
    case 0:
	printf("no commands matched\n");
	break;
    case 1:
	found_action(buf);
	break;
    default:
	printf("More than one command matched\n");
	break;
    }
}

int
main ()
{
    char *line;

    linenoiseSetCompletionCallback([] (const char *buf, linenoiseCompletions *lc) {
	    command_match(buf, [lc] (command_t &cmd) {
		    linenoiseAddCompletion(lc, cmd.c_token, cmd.c_help);
		});
	});
    linenoiseHistoryLoad("history.txt");

    /*
     * Now this is the main loop of the typical linenoise-based application.
     * The call to linenoise() will block as long as the user types something
     * and presses enter.
     *
     * The typed string is returned as a malloc() allocated string by
     * linenoise, so the user needs to free() it. 
     */
    while ((line = linenoise("computer> ")) != NULL) {
	if (strlen(line)) {
	    call_command(line);

	    linenoiseHistoryAdd(line);
	    linenoiseHistorySave("history.txt");
	}


        free(line);
    }
    return 0;
}
