#include "./headers/vmSupport.h"
#include "./headers/sysSupport.h"
#include "./headers/initProc.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>
extern char _end;

/* Indirizzo di partenza della Swap Pool*/

//#define SWAPPOOLSTART (RAMSTART + (OSFRAMES * PAGESIZE))
#define SWAPPOOLSTART (((memaddr)&_end+PAGESIZE-1) & ~(PAGESIZE-1)) 

/* Indirizzo del device register di un flash device (stessa formula di phase2/interrupts.c
 * per IntlineNo=4, senza toccare quel file: START_DEVREG + (IntlineNo-3)*0x80 + devNo*0x10). */
//MACRO SOLO PER I FLASH DEVICE, NON PER TUTTI I DEVICE, PER QUESTO NON CE intline-3. 0x80 mi serve per saltare direttamente alla zona dei flash device, che sono 8 e partono da IntlineNo=4. Quindi IntlineNo-3 = 1, e 1*0x80 = 0x80. Poi aggiungo devNo*0x10 per saltare al registro del flash device corrispondente all'ASID dell'U-proc.
#define FLASHDEVADDR(devNo) (START_DEVREG + 0x80 + ((devNo) * 0x10))
// il device register di un flash contiene 4 registri: command, status, data0 e data1.  
/* Swap Pool Table: locale al modulo, come richiesto in sez. 12.2 delle specifiche. */
static swap_t swapPoolTable[POOLSIZE]; // 16 caselle di swapool in memoria
static int    swapPoolSem; // sem per accesso mutex alla swap pool table
static int    nextVictimFrame; /* puntatore FIFO round-robin (sez. 5.4) */

// Init: frame swap pool liberi
void initSwapStructs(void) {
    for (int i = 0; i < POOLSIZE; i++) {
        swapPoolTable[i].sw_asid   = -1; // frame liberi
        swapPoolTable[i].sw_pageNo = 0; // vpn
        swapPoolTable[i].sw_pte    = NULL;
    }
    swapPoolSem     = 1;
    nextVictimFrame = 0;
}

//funzione che libera i frame della swap pool occupati da un processo utente con un dato ASID
void freeUprocFrames(int asid) {
    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0);
    for (int i = 0; i < POOLSIZE; i++) {
        if (swapPoolTable[i].sw_asid == asid) {
            swapPoolTable[i].sw_asid   = -1;
            swapPoolTable[i].sw_pageNo = 0;
            swapPoolTable[i].sw_pte    = NULL;
        }
    }
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0); // libero mutex
}

static int pickFrame(void) {
    for (int i = 0; i < POOLSIZE; i++) {
        if (swapPoolTable[i].sw_asid == -1) {
            return i;
        }
    }

    /* nessun frame libero: fallback al round-robin originale tra gli occupati */
    int frame       = nextVictimFrame;
    nextVictimFrame = (nextVictimFrame + 1) % POOLSIZE;
    return frame;
}

/* Legge/scrive un blocco (0..31) sul flash device dell'U-proc asid, sotto mutua
 * esclusione. Ritorna lo status del device (sez. 5.1). */
static int flashOperation(int asid, int blockNo, memaddr frameAddr, int writeOp) {
    int       devNo    = asid - 1; /* ASID 1..8 -> flash device 0..7  sottraendo 1 agli ASID ottengo l'indice del flash device. ad es la shell che ha asid 1 ha indice 0 per il flash device (file config_machine)*/
    dtpreg_t *flashReg = (dtpreg_t *)FLASHDEVADDR(devNo); // puntatore al registro del flash device corrispondente all'ASID dell'U-proc, calcolato con la macro FLASHDEVADDR(devNo)
    int       status;

    SYSCALL(PASSEREN, (int)&flashMutex[devNo], 0, 0); // acquisizione del semaforo per l'accesso al flash device, per garantire mutua esclusione
    flashReg->data0 = frameAddr; //scrivo l'indirizzo del frame nella memoria del flash device, in modo che il flash device sappia dove leggere/scrivere i dati
    status = SYSCALL(DOIO, (int)&flashReg->command, (blockNo << 8) | (writeOp ? FLASHWRITE : FLASHREAD), 0);
    SYSCALL(VERHOGEN, (int)&flashMutex[devNo], 0, 0);//rilascio il semaforo per l'accesso al flash device, in modo che altri processi possano accedere al flash device

    return status;
}


void tlbUpdate(unsigned int entryHI, unsigned int entryLO) {
    setENTRYHI(entryHI); //scrivo nel registro ENTRYHI il valore dell'indirizzo virtuale della pagina e l'ASID del processo, in modo che il processore sappia quale pagina virtuale e quale processo stiamo aggiornando nella TLB
    TLBP();//cerca nella TLB l'entry corrispondente all'indirizzo virtuale della pagina e all'ASID del processo, in modo da sapere se la pagina è già presente nella TLB o meno
    if ((getINDEX() & PRESENTFLAG) == 0) { //leggo il registro INDEX per sapere se la pagina è già presente nella TLB o meno. se il bit PRESENTFLAG è spento= è presente nella TLB 
        setENTRYLO(entryLO);//se trovo la pagina nella TLB, scrivo nel registro ENTRYLO il nuovo valore dell'indirizzo fisico della pagina e i bit di validità e protezione, in modo che il processore sappia quale pagina fisica stiamo aggiornando nella TLB
        TLBWI(); //scrivo entryHI e entryLO nella riga che Index ha trovato in modo da aggiornare SOLO l'entry corrispondente nella TLB, senza svuotare tutta la TLB
    }
}

/* Converte il VPN mancante nell'indice (0..31) della Page Table privata (sez. 2.1). */
//quando avviene un page fault, ti dice solo l'indirizzo virtuale mancante, ma per aggiornare la page table del processo utente dobbiamo sapere l'indice della page table.
static int pageIndexFromVPN(unsigned int vpn) {
    if (vpn == 0xBFFFF) //se il VPN è 0xBFFFF, significa che il processo utente sta cercando di accedere all'ultima pagina della sua area di memoria virtuale (quella che contiene lo stack), quindi l'indice della page table privata corrispondente è USERPGTBLSIZE - 1 (31)
        return USERPGTBLSIZE - 1;
    return vpn - (KUSEG >> VPNSHIFT); // KUSEG >> VPNSHIFT è il numero di pagina virtuale della prima pagina della memoria virtuale del processo utente. sottraendo questo valore al VPN mancante ottiengo l'indice della page table privata corrispondente.
}


/*  i 14 passi della sez. 4.2 delle specifiche. */
void pager(void) {
    support_t   *supp; //puntatore alla support structure del processo utente che ha generato il page fault, ottenuto tramite la syscall GETSUPPORTPTR. 
    state_t     *excState; //puntatore allo stato della CPU al momento della page fault
    unsigned int excCode;
    unsigned int vpn;
    int          p;
    int          frame;
    memaddr      frameAddr;
    int          status;

    supp     = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* passo 1 : ci da l'indirizzo della support structure del processo che ha generato la page fault. questa struttura contiene la page table e gli stati di eccezione  */
    excState = &supp->sup_exceptState[PGFAULTEXCEPT]; // qui prendiamo lo stato della CPU al momento della page fault, che contiene informazioni come il cause register, l'entry_hi register, ecc. Queste informazioni ci servono per capire quale pagina virtuale ha causato la page fault e quale eccezione è stata generata.
    excCode  = excState->cause & CAUSE_EXCCODE_MASK;         /* passo 2: estrae il codice di eccezione dal cause register che dice il tipo di eccezione, ripulendo i bit non significativi con AND */

    if (excCode == EXC_TLBMOD) { /* passo 3: se l'errore è un tentativo  di scrivere in una pagina segnata come "solo lettura"*/
        supportProgramTrapHandler(supp);//lo trattiamo come un program trap e lo delego al gestore dei program trap del support level, che termina il processo utente in modo ordinato.
        return;
    }

    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0); /* passo 4: acquisisce il semaforo del swap pool */

    
    vpn = excState->entry_hi >> VPNSHIFT;
    p   = pageIndexFromVPN(vpn); /* passo 5 */

    frame     = pickFrame(); /* passo 6 */
    frameAddr = SWAPPOOLSTART + (frame * PAGESIZE);//calcolo l'indirizzo fisico del frame scelto nella swap pool, che si trova a partire dall'indirizzo SWAPPOOLSTART e ha dimensione PAGESIZE. Quindi frameAddr è l'indirizzo fisico del frame scelto nella swap pool.

    if (swapPoolTable[frame].sw_asid != -1) { /* passi 7-8: frame occupato */
        int         victimAsid = swapPoolTable[frame].sw_asid;
        int         victimPage = swapPoolTable[frame].sw_pageNo;
        pteEntry_t *victimPte  = swapPoolTable[frame].sw_pte; // sw_pte è un puntatore alla riga della page table del processo che ha occupato il frame scelto, che contiene l'indirizzo virtuale della pagina e l'ASID del processo. Lo uso per invalidare la PTE e aggiornare la TLB.

        /* 8a+8b: invalidazione PTE e aggiornamento TLB atomici (sez. 10:
         * TLBP+TLBWI mirati sulla sola entry della vittima, invece di
         * TLBCLR() . */
        setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); //operazione atomica: disabilito gli interrupt globali per evitare che un altro processo possa modificare la PTE o la TLB mentre sto aggiornando la PTE del processo vittima.
        victimPte->pte_entryLO &= ~VALIDON;//spengo il bit di validità della PTE del processo vittima, in modo che il processore sappia che la pagina non è più valida e generi un page fault se il processo vittima cerca di accedere a quella pagina.
        tlbUpdate(victimPte->pte_entryHI, victimPte->pte_entryLO);//aggiorno la TLB con il nuovo valore della PTE del processo vittima, in modo che il processore sappia che la pagina non è più valida e generi un page fault se il processo vittima cerca di accedere a quella pagina.
        setSTATUS(getSTATUS() | MSTATUS_MIE_MASK); //riabilito gli interrupt globali, in modo che altri processi possano eseguire le loro operazioni.
        status = flashOperation(victimAsid, victimPage, frameAddr, TRUE); /* 8c : operazione di scrittura sulla flash (true=scrittura).*/
        if (status != READY) { //se l'operazione di scrittura sulla flash non è andata a buon fine, rilascio il semaforo della swap pool e delego al gestore dei program trap del support level, che termina il processo utente in modo ordinato.
            SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
            supportProgramTrapHandler(supp); 
            return;
        }
    }

    status = flashOperation(supp->sup_asid, p, frameAddr, FALSE); /* passo 9: operazione di lettura sulla flash del processo che ha causato il page fault la pagina p che serviva, caricandola nel frame appena liberato.*/
    if (status != READY) {
        SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
        supportProgramTrapHandler(supp);
        return;
    }

    /* passo 10:aggiorno la swap pool */
    swapPoolTable[frame].sw_asid   = supp->sup_asid; //aggiorno l'ASID del processo nel frame della swap pool
    swapPoolTable[frame].sw_pageNo = p;//aggiorno il numero di pagina virtuale del processo nel frame della swap pool
    swapPoolTable[frame].sw_pte    = &supp->sup_privatePgTbl[p]; //aggiorno il puntatore alla PTE del processo nel frame della swap pool, in modo che il pager sappia quale PTE invalidare quando il frame viene sostituito.

    /* passi 11+12: aggiornamento PTE corrente e TLB atomici. */
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); //disabilito gli interrupt globali per evitare che un altro processo possa modificare la PTE o la TLB mentre sto aggiornando la PTE del processo che ha causato il page fault.
    supp->sup_privatePgTbl[p].pte_entryLO = (frameAddr & 0xFFFFF000) | DIRTYON | VALIDON; //aggiorno la page table entry del processo che ha causato il page fault, impostando l'indirizzo fisico del frame appena caricato, e accendendo i bit di validità e di scrittura (dirty).
    tlbUpdate(supp->sup_privatePgTbl[p].pte_entryHI, supp->sup_privatePgTbl[p].pte_entryLO); //aggiorno la TLB con il nuovo valore della PTE del processo che ha causato il page fault, in modo che il processore sappia che la pagina è valida e possa accedervi senza generare un page fault.
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);//riabilita gli interrupt

    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0); /* passo 13:rilascio il semaforo della swap pool */

    LDST(excState); /* passo 14: ripristino lo stato della CPU */
}
