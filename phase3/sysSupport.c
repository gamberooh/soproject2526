#include "./headers/sysSupport.h"
#include "./headers/initProc.h"
#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>

/* Indirizzo del device register del terminale (stessa formula di phase2/interrupts.c
 * per IntlineNo=7). In Fase 3 e' usato un solo terminale. */
#define TERMDEVADDR(devNo) (START_DEVREG + 0x200 + ((devNo) * 0x10)) //calcola l'indirizzo dei registri del terminale numero devNo. x200 è l'offset per saltare alla zona dei registri dei terminali, che sono 8 e partono da IntlineNo=7. Quindi IntlineNo-3 = 4, e 4*0x80 = 0x200. Poi aggiungo devNo*0x10 per saltare al registro del terminale corrispondente al numero del terminale.

static void sysWriteTerminal(support_t *supp);
static void sysReadTerminal(support_t *supp);
static void sysExecute(support_t *supp);
static int  isValidUserAddr(memaddr addr);

/* Termina in modo ordinato lo U-proc descritto da supp (sez. 7.1 e sez. 9). */
void terminateUproc(support_t *supp) {
    int asid = supp->sup_asid;
    freeUprocFrames(asid); //libero tutti i frame della swap pool occupati dal processo che sta terminando, in modo che possano essere riutilizzati da altri processi.
    freeSupportStruct(supp);// libero la struttura di supporto che usa il processo che sta terminando

    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); //disabilito gli interrupt globali per evitare che un altro processo possa modificare la PTE o la TLB mentre sto invalidando le entry della TLB del processo che sta terminando. deve essere atomico
    for (int i = 0; i < USERPGTBLSIZE; i++) { //per ogni pagina della page table privata del processo che sta terminando,
        if (supp->sup_privatePgTbl[i].pte_entryLO & VALIDON) { //se la pagina è valida, allora invalido la entry corrispondente nella TLB, in modo che il processore sappia che la pagina non è più valida e generi un page fault se il processo successivo con lo stesso ASID cerca di accedere a quella pagina.
            tlbUpdate(supp->sup_privatePgTbl[i].pte_entryHI,
                      supp->sup_privatePgTbl[i].pte_entryLO & ~VALIDON);
        }
    }
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

    if (asid == 1) { //se è la shell, sveglio l'istantiatorProcess (master) che stava aspettando su masterSemaphore,
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else { //se è un altro programma (lanciato dalla shell), sveglio la shell che stava aspettando su shellSemaphore
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }

    SYSCALL(TERMPROCESS, 0, 0, 0);//rimuove il processo
}

/* Program Trap: stessa procedura di un SYS2 (sez. 8). */
void supportProgramTrapHandler(support_t *supp) {
    terminateUproc(supp);
}
//funzione che controlla se un indirizzo è valido per l'utente, cioè se è compreso tra KUSEG e USERSTACKTOP
static int isValidUserAddr(memaddr addr) {
    return (addr >= KUSEG) && (addr < USERSTACKTOP);
}
//funzione che gestisce le syscall, SYS2 (terminate), SYS4 (writeTerminal), SYS5 (readTerminal) e SYS6 (execute).
static void supportSyscallHandler(support_t *supp) {
    state_t *state = &supp->sup_exceptState[GENERALEXCEPT]; //salva lo stato dei registri della CPU al momento della syscall, che contiene informazioni come il cause register, l'entry_hi register, ecc
    state->pc_epc += 4; /* sez. 7: PC avanzato prima del dispatch, dato che qiesto registro contiene l'indirizzo della prossima istruzione da eseguire( conteneva prima la syscall e quindi se non la modificavo di +4, avrebbe eseguito la syscall di nuovo) */

    switch ((int)state->reg_a0) { // nel registro a0 c'è il numero della syscall invocata, quindi faccio uno switch su questo numero per capire quale syscall è stata invocata e quindi quale funzione chiamare.
    case TERMINATE:
        terminateUproc(supp); /* non ritorna */
        return;
    case WRITETERMINAL:
        sysWriteTerminal(supp);
        break;
    case READTERMINAL:
        sysReadTerminal(supp);
        break;
    case EXECUTE:
        sysExecute(supp); /* fa gia' LDST internamente */
        return;
    default:
        terminateUproc(supp); /*   termino il processo per sicurezza se il codice in a0 non è valido */
        return;
    }

    LDST(state);//ricarico lo stato aggiornato del processo nella CPU, in modo che il processo possa continuare la sua esecuzione dopo la syscall.
}
//funzione che gestisce le eccezioni generali, cioè le syscall e le eccezioni di programma. In base al cause register, decide se chiamare la funzione di gestione delle syscall o quella di gestione delle eccezioni di programma.
void supportGeneralExceptionHandler(void) {
    support_t   *supp = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); //prende il puntatore alla struttura di supporto del processo corrente(che ha fatto l'eccezione)
    unsigned int cause = supp->sup_exceptState[GENERALEXCEPT].cause & CAUSE_EXCCODE_MASK;//estraggo dal registro cause il codice dell'eccezione, che mi dice quale eccezione è stata generata( syscall o eccezione di programma)

    if (cause == EXC_ECU || cause == EXC_ECM) { //EXC_ECU  e EXC_ECM sono i codici delle eccezioni di syscall, quindi se il cause register contiene uno di questi due valori, significa che è stata invocata una syscall e quindi chiamo la funzione di gestione delle syscall.
        supportSyscallHandler(supp);
    } else {
        supportProgramTrapHandler(supp);//altrimenti chiamo la funzione di gestione delle eccezioni di programma.
    }
}

/* SYS4 - WriteTerminal (sez. 7.2) */
static void sysWriteTerminal(support_t *supp) {
    state_t   *state = &supp->sup_exceptState[GENERALEXCEPT]; //salvo lo stato del processo al momento della syscall
    char      *vaddr = (char *)state->reg_a1; //in a1 ce l'indirizzo del buffer da cui leggere i dati da scrivere sul terminale
    int        len   = (int)state->reg_a2;//in a2 ce la lunghezza del buffer da leggere
    termreg_t *termReg; //termreg_t è una struttura che rappresenta i registri del terminale che contiene 
    int        count;
    int        status;

    if (len < 0 || len > MAXSTRLENG || !isValidUserAddr((memaddr)vaddr) || !isValidUserAddr((memaddr)vaddr + len)) {//controllo se i dati sono sensati: la lunghezza deve essere positiva e minore di MAXSTRLENG, e l'indirizzo del buffer deve essere valido per l'utente(vaddr è il punto di partenza del buffer e vaddr + len è il punto finale del buffer, quindi entrambi devono essere validi per l'utente)
        terminateUproc(supp);
        return;
    }

    termReg = (termreg_t *)TERMDEVADDR(0); //prendo l'indirizzo dei registri del terminale numero 0, che è l'unico terminale disponibile in fase 3. TERMDEVADDR(0) calcola l'indirizzo dei registri del terminale numero 0

    SYSCALL(PASSEREN, (int)&termWriteMutex, 0, 0);//acquisisce il mutex del terminale per evitare che più processi scrivano sul terminale contemporaneamente.

    count  = 0; //inizializzo il contatore dei caratteri scritti a 0
    status = OKCHARTRANS;//inizializzo lo stato della trasmissione a OKCHARTRANS, che significa che il terminale è pronto a trasmettere un carattere.
    while (count < len && (status & 0xFF) == OKCHARTRANS) { //finche non abbiamo mandato tutti i caratteri e (stato & 0xFF) == OKCHARTRANS significa che il terminale è pronto a trasmettere un carattere, quindi possiamo continuare a scrivere sul terminale. lo status restituito dal terminale è un intero a 32 bit, ma i primi 8 bit contengono lo stato della trasmissione, quindi faccio un AND con 0xFF per prendere solo i primi 8 bit. se questo stato equivale a OKCHARTRANS, significa che il terminale è pronto a trasmettere un carattere.
        status = SYSCALL(DOIO, (int)&termReg->transm_command, (vaddr[count] << 8) | TRANSMITCHAR, 0);
        //salvo in status il risultato della syscall DOIO
        //(int)&termReg->transm_command è l'indirizzo del registro del terminale che gestisce la trasmissione dei caratteri.
        if ((status & 0xFF) == OKCHARTRANS)
            count++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteMutex, 0, 0); //rilascio il mutex del terminale, in modo che altri processi possano scrivere sul terminale.

    state->reg_a0 = ((status & 0xFF) == OKCHARTRANS) ? count : -status;
}

/* SYS5 - ReadTerminal (sez. 7.3) */
static void sysReadTerminal(support_t *supp) {
    state_t   *state = &supp->sup_exceptState[GENERALEXCEPT]; //salvo lo stato del processo al momento della syscall
    char      *vaddr = (char *)state->reg_a1;//in a1 ce l'indirizzo del buffer dove vuole ricevere il testo letto dal terminale
    termreg_t *termReg;
    int        count;
    int        status;
    char       ch = 0;

    if (!isValidUserAddr((memaddr)vaddr)) { //se l'indirizzo del buffer non è valido per l'utente, termino il processo. 
        terminateUproc(supp);
        return;
    }

    termReg = (termreg_t *)TERMDEVADDR(0);//prendo l'indirizzo dei registri del terminale numero 0, che è l'unico terminale disponibile in fase 3. 

    SYSCALL(PASSEREN, (int)&termReadMutex, 0, 0); //acquisisce il mutex del terminale per evitare che più processi leggano dal terminale contemporaneamente.

    count = 0;
    do {
        status = SYSCALL(DOIO, (int)&termReg->recv_command, RECEIVECHAR, 0); //(int)&termReg->recv_command è l'indirizzo del registro del terminale che gestisce la ricezione dei caratteri, RECEIVECHAR è il comando che indica al terminale di ricevere un carattere. 
        if ((status & 0xFF) == CHARRECV) {//se lo status restituito dalla syscall DOIO è uguale a CHARRECV, significa che il terminale ha ricevuto un carattere e possiamo salvarlo nel buffer. 
            ch          = (char)((status >> 8) & 0xFF); 
            vaddr[count] = ch; //salvo il carattere ricevuto nel buffer, all'indice count.
            count++;
        }
    } while ((status & 0xFF) == CHARRECV && ch != '\n' && count < MAXSTRLENG);//finche la lettura va bene e il carattere letto non è a capo linea e non abbiamo letto più di MAXSTRLENG caratteri, continuiamo a leggere dal terminale.

    SYSCALL(VERHOGEN, (int)&termReadMutex, 0, 0); //rilascio il mutex del terminale, in modo che altri processi possano leggere dal terminale.

    state->reg_a0 = ((status & 0xFF) == CHARRECV) ? count : -status;//prepara il valore di ritorno della syscall, che sarà il numero di caratteri letti se la ricezione è andata a buon fine, oppure un valore negativo che indica l'errore se la ricezione è fallita. lo stato restituito dal terminale è un intero a 32 bit, ma i primi 8 bit contengono lo stato della ricezione, quindi faccio un AND con 0xFF per prendere solo i primi 8 bit. se questo stato equivale a CHARRECV, significa che il terminale ha ricevuto correttamente tutti i caratteri e quindi il valore di ritorno sarà il numero di caratteri letti(count). altrimenti, il valore di ritorno sarà -status, che indica l'errore.
}

/* SYS6 - Execute (sez. 7.4): solo la shell (ASID 1) puo' invocarlo.funzione che permette alla shell di lanciare ed eseguire un nuovo programma */
static void sysExecute(support_t *supp) {
    state_t    *state = &supp->sup_exceptState[GENERALEXCEPT]; //salvo lo stato del processo al momento della syscall
    int         asid;
    state_t     newState;
    support_t  *newSupp;

    if (supp->sup_asid != 1) { //se il processo che ha invocato la syscall non è la shell (ASID 1), termino il processo. 
        terminateUproc(supp);
        return;
    }

    asid    = (int)state->reg_a1;//leggo dal registro a1 il numero di ASID del nuovo processo da creare
    newSupp = allocSupportStruct(); //alloco una nuova struttura di supporto per il nuovo processo da creare. 
    if (newSupp == NULL) {
        LDST(state);
        return;
    }
    initUprocState(&newState, asid);// inizializzo lo stato del nuovo processo da creare, impostando i registri della CPU e la memoria virtuale del nuovo processo.
    initUprocSupport(newSupp, asid);// inizializzo la struttura di supporto del nuovo processo da creare, impostando i registri della CPU e la memoria virtuale del nuovo processo.

    SYSCALL(CREATEPROCESS, (int)&newState, PROCESS_PRIO_LOW, (int)newSupp);//creo il nuovo processo, passando come parametri lo stato del nuovo processo, la priorità del nuovo processo e la struttura di supporto del nuovo processo. 

    SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);//metto in attesa la shell, che si sveglierà quando il nuovo processo terminerà. 

    LDST(state);//ricarico lo stato aggiornato del processo nella CPU, in modo che il processo possa continuare la sua esecuzione dopo la syscall. 
}
