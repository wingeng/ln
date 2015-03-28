#include <unistd.h>
#include <string.h>
#include "linenoise.h"


int
main ()
{
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
	   "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (lnEnableRawMode(STDIN_FILENO) == -1) return -1;

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
