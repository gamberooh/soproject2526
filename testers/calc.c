    #include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

#define MAXLINE 16

static void printInt(int n);

void main() {
    char buf[MAXLINE + 1];
    int  status;
    int  a, b;
    char op;
    int  result = 0;
    int  valid;

    print(WRITETERMINAL, "calc: enter <digit><op><digit>, e.g. 3+5\n");

    status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
    /* rimuove il newline finale */
    if (status > 0 && buf[status - 1] == '\n')
        status--;
    buf[status] = EOS;

    /* validazione dell'input */
    valid = (status == 3) && (buf[0] >= '0' && buf[0] <= '9') && (buf[2] >= '0' && buf[2] <= '9') &&
            (buf[1] == '+' || buf[1] == '-' || buf[1] == '*' || buf[1] == '/');

    if (!valid) {
        print(WRITETERMINAL, "Invalid input\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    a  = buf[0] - '0';
    op = buf[1];
    b  = buf[2] - '0';

    switch (op) {
    case '+':
        result = a + b;
        break;
    case '-':
        result = a - b;
        break;
    case '*':
        result = a * b;
        break;
    case '/':
        if (b == 0) {
            print(WRITETERMINAL, "Error: division by zero\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
        result = a / b;
        break;
    }

    print(WRITETERMINAL, "Result: ");
    printInt(result);
    print(WRITETERMINAL, "\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}

/* stampa un intero con segno sul terminale */
static void printInt(int n) {
    char buf[12];
    char c[2];
    int  i   = 0;
    int  neg = 0;

    if (n < 0) {
        neg = 1;
        n   = -n;
    }

    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }

    if (neg)
        buf[i++] = '-';

    c[1] = EOS;
    /* stampa i caratteri invertiti */
    while (i > 0) {
        i--;
        c[0] = buf[i];
        print(WRITETERMINAL, c);
    }
}
