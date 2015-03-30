/* linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 */

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
#include <string>
#include <functional>
#include <vector>

class lnCompletion;

typedef std::vector<lnCompletion> lnCompletionVec;
typedef std::function<void (const char *, lnCompletionVec *)> lnCompletionFunc;

void lnSetCompletionCallback(lnCompletionFunc fn);
void lnAddCompletion(lnCompletionVec *, const char *, const char *);

#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#define UNUSED __attribute__((unused))

#endif /* __LINENOISE_H */
