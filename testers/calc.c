#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

#define MAXLINE 16 // lunghezza massima di una riga di input della shell


static void printInt(int n);

void main() {
    char buf[MAXLINE + 1];
    int  status;// variabile per memorizzare lo stato della syscall READTERMINAL
    int  a, b; // operandi dell'operazione aritmetica
    char op;// operatore aritmetico (+, -, *, /)
    int  result = 0;
    int  valid;

    print(WRITETERMINAL, "calc: enter <digit><op><digit>, e.g. 3+5\n"); // stampa sul terminale un messaggio che indica all'utente come utilizzare il programma calc

    status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0); // chiama la syscall READTERMINAL per leggere una riga di input dal terminale e salvarla nel buffer buf. il primo parametro è l'indirizzo del buffer dove salvare la riga letta, gli altri due parametri sono 0 perché non servono in questa syscall. il valore di ritorno della syscall è salvato in status, che indica il numero di caratteri letti o un codice di errore se la lettura è fallita
    if (status > 0 && buf[status - 1] == '\n') //se la lettura è andata a buon fine e l'ultimo carattere letto è un newline(che intende dire che l'utente ha premuto invio per avviare il comando)
        status--;
    buf[status] = EOS;//metto il carattere di fine stringa (EOS) alla fine della riga letta, in modo che la shell possa trattare correttamente la riga come una stringa C standard.
    //controlla se l'input è valido: deve essere lungo almeno 3 caratteri, il primo e il terzo devono essere cifre (0-9) e il secondo deve essere un operatore aritmetico valido (+, -, *, /)
    valid = (status == 3) && (buf[0] >= '0' && buf[0] <= '9') && (buf[2] >= '0' && buf[2] <= '9') &&
            (buf[1] == '+' || buf[1] == '-' || buf[1] == '*' || buf[1] == '/');

    if (!valid) {
        print(WRITETERMINAL, "Invalid input\n");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    a  = buf[0] - '0'; //converte il primo carattere della stringa in un intero (ad esempio, '3' diventa 3). faccio - '0' perché in ASCII i caratteri numerici sono consecutivi e partono da '0' (48 in decimale).
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

    print(WRITETERMINAL, "Result: "); //stampa sul terminale il messaggio "Result: " 
    printInt(result);//stampa sul terminale il risultato dell'operazione aritmetica calcolata dalla shell
    print(WRITETERMINAL, "\n");//stampa sul terminale un newline per andare a capo dopo il risultato

    SYSCALL(TERMINATE, 0, 0, 0);//termina il processo calc, che è l'ultimo processo in esecuzione.
}

/* Stampa un intero con segno sul terminale, un carattere alla volta.*/
static void printInt(int n) {
    char buf[12]; // buffer per memorizzare la rappresentazione in stringa dell'intero n. 12 è sufficiente per rappresentare un intero con segno a 32 bit
    char c[2];// buffer per memorizzare un singolo carattere da stampare sul terminale. 2 è sufficiente per memorizzare un carattere e il terminatore di stringa
    int  i   = 0;
    int  neg = 0;

    if (n < 0) {//se il numero è negativo, imposto neg=1 e converto n in positivo per semplificare la stampa delle cifre. In questo modo, posso stampare il segno negativo separatamente alla fine.
        neg = 1;
        n   = -n;
    }
    //se n è 0, aggiungo '0' al buffer buf. 
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10); //'0' converte una cifra numerica in un carattere ASCII. es '0'+3='3'. mentre n modulo 10 restituisce l'ultima cifra di n. quindi aggiungo questa cifra al buffer buf come carattere. Poi divido n per 10 per rimuovere l'ultima cifra e ripeto il processo finché n non diventa 0.
            n /= 10;
        }
    }

    if (neg)
        buf[i++] = '-';//aggiungo il segno negativo al buffer buf se il numero originale era negativo. 

    c[1] = EOS; //aggiungo il terminatore di stringa al buffer c, in modo che possa essere stampato correttamente come una stringa C standard.
    while (i > 0) { //stampo i caratteri del buffer buf in ordine inverso (perché le cifre sono state aggiunte al buffer in ordine inverso). Decremento i e stampo il carattere corrispondente dal buffer buf.
        i--;
        c[0] = buf[i];
        print(WRITETERMINAL, c);
    }
}
