/* linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 */

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *linenoiseCompletions;
typedef void (linenoiseCompletionFunc)(const char *, linenoiseCompletions *);

void linenoiseSetCompletionCallback(linenoiseCompletionFunc fn);
void linenoiseAddCompletion(linenoiseCompletions, const char *, const char *);

char *linenoise(const char *prompt);
int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);

int lnEnableRawMode(int);
void lnDisableRawMode(int);

#ifdef __cplusplus
}
#endif


#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#define UNUSED __attribute__((unused))

#endif /* __LINENOISE_H */
