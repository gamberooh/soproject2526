#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

#define MAXLINE  128
#define NUMPROGS 7

static char *progNames[NUMPROGS] = {"date", "echo", "fibEight", "fibEleven", "uname", "calc", "sl"};
static char *progNames[NUMPROGS] = {
    "fibEight",
    "echo",
    "fibEleven",
    "uname",
    "date",
    "sl",
    "calc"
};
static int progAsid[NUMPROGS] = {2, 3, 4, 5, 6, 7, 8};

static int streq(char *a, char *b);

void main() {
    char buf[MAXLINE + 1];
    int  status;
    int  i;
    int  found;

    for (;;) {
        print(WRITETERMINAL, "$ ");

        status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
        if (status < 0) {
            print(WRITETERMINAL, "Read error\n");
            continue;
        }

        /* rimuove il newline finale */
        if (status > 0 && buf[status - 1] == '\n')
            status--;
        buf[status] = EOS;

        if (status == 0)
            continue;

        if (streq(buf, "exit"))
            break;

        /* cerca ed esegue il comando */
        found = 0;
        for (i = 0; i < NUMPROGS; i++) {
            if (streq(buf, progNames[i])) {
                found = 1;
                SYSCALL(EXECUTE, progAsid[i], 0, 0);
                break;
            }
        }

        if (!found)
            print(WRITETERMINAL, "Unknown command\n");
    }

    SYSCALL(TERMINATE, 0, 0, 0);
}

/* confronto tra due stringhe */
static int streq(char *a, char *b) {
    int i = 0;
    while (a[i] != EOS && b[i] != EOS) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == EOS && b[i] == EOS;
}
