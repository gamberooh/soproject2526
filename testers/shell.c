#include <uriscv/liburiscv.h>

#include "h/tconst.h"
#include "h/print.h"

#define MAXLINE  128
#define NUMPROGS 7


static char *progNames[NUMPROGS] = {"date", "echo", "fibEight", "fibEleven", "uname", "calc", "sl"};
static int   progAsid[NUMPROGS]  = {2, 3, 4, 5, 6, 7, 8};


static int streq(char *a, char *b);

void main() {
    char buf[MAXLINE + 1]; //+1 per il terminatore di stringa EOS. buffer dove la shell legge i comandi da terminale
    int  status;
    int  i;
    int  found;

    for (;;) {
        print(WRITETERMINAL, "$ ");

        status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0); //legge una riga di input dal terminale e la salva nel buffer buf. il valore di ritorno della syscall è il numero di caratteri letti, oppure un valore negativo che indica l'errore se la lettura è fallita. (int)&buf[0] è l'indirizzo del buffer dove salvare la riga letta dal terminale. 
        if (status < 0) { //se la lettura dal terminale è fallita, stampo un messaggio di errore e continuo il ciclo della shell.
            print(WRITETERMINAL, "Read error\n");
            continue;
        }

        /* rimuove il newline finale letto da SYS5 */
        if (status > 0 && buf[status - 1] == '\n') //se l'ultimo carattere letto è un newline, lo rimuovo sostituendolo con il terminatore di stringa EOS. cosi in C posso trattare la stringa come una normale stringa terminata da EOS.
            status--; 
        buf[status] = EOS;

        if (status == 0) //se la riga letta è vuota, continuo il ciclo della shell.
            continue;

        if (streq(buf, "exit")) //se il comando letto è "exit", esco dal ciclo della shell e termino il processo.
            break;

        found = 0;
        for (i = 0; i < NUMPROGS; i++) { //ciclo su tutti i programmi conosciuti dalla shell, confrontando il comando letto con i nomi dei programmi nella tabella progNames. se trovo una corrispondenza, invoco la syscall SYS6 per eseguire il programma corrispondente.
            if (streq(buf, progNames[i])) {
                found = 1;
                SYSCALL(EXECUTE, progAsid[i], 0, 0);
                break;
            }
        }

        if (!found) //se il comando letto non corrisponde a nessun programma conosciuto dalla shell, stampo un messaggio di errore.
            print(WRITETERMINAL, "Unknown command\n");
    }

    SYSCALL(TERMINATE, 0, 0, 0); //termina il processo
}
//funzione che confronta due stringhe a e b, restituendo 1 se sono uguali e 0 altrimenti.
static int streq(char *a, char *b) {
    int i = 0;
    while (a[i] != EOS && b[i] != EOS) { //finche non arrivo alla fine di una delle due stringhe, confronto i caratteri uno per uno
        if (a[i] != b[i]) //se trovo un carattere diverso, le stringhe non sono uguali e restituisco 0
            return 0;
        i++;
    }
    return a[i] == EOS && b[i] == EOS; //serve a evitare che una stringa sia prefisso dell'altra, ad esempio "echo" e "echoo". 
}
