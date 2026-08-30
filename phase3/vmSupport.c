#include "./headers/vmSupport.h"
#include "./headers/sysSupport.h"
#include "./headers/initProc.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>
extern char _end;

/* Indirizzo di partenza della Swap Pool*/

#define SWAPPOOLSTART (((memaddr)&_end+PAGESIZE-1) & ~(PAGESIZE-1)) 

#define FLASHDEVADDR(devNo) (START_DEVREG + 0x80 + ((devNo) * 0x10)) // flash device address resolution macro

static swap_t swapPoolTable[POOLSIZE]; // 16 caselle di swapool in memoria
static int    swapPoolSem; // sem per accesso mutex alla swap pool table
static int    nextVictimFrame; // puntatore FIFO round-robin

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
    // ottimizzazione I/O: ricerca del prima asid frame libero
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
 * esclusione. Ritorna lo status del device  */
static int flashOperation(int asid, int blockNo, memaddr frameAddr, int writeOp) {
    int       devNo    = asid - 1; 
    dtpreg_t *flashReg = (dtpreg_t *)FLASHDEVADDR(devNo); // registro del flash device corrispondente
    int       status;

    SYSCALL(PASSEREN, (int)&flashMutex[devNo], 0, 0); // mutex per acquisizione del flash device
    flashReg->data0 = frameAddr;
    status = SYSCALL(DOIO, (int)&flashReg->command, (blockNo << 8) | (writeOp ? FLASHWRITE : FLASHREAD), 0);
    SYSCALL(VERHOGEN, (int)&flashMutex[devNo], 0, 0);

    return status;
}


void tlbUpdate(unsigned int entryHI, unsigned int entryLO) {
    setENTRYHI(entryHI); 
    TLBP(); // cerco sulla base del vpn corrente
    if ((getINDEX() & PRESENTFLAG) == 0) { 
        setENTRYLO(entryLO);
        TLBWI();
    }
}

/* Converte il VPN mancante nell'indice della Page Table privata */
static int pageIndexFromVPN(unsigned int vpn) {
    if (vpn == 0xBFFFF)
        return USERPGTBLSIZE - 1;
    return vpn - (KUSEG >> VPNSHIFT); // KUSEG >> VPNSHIFT vpn prima pagina vm
}


void pager(void) {
    support_t   *supp; // punt al support structure del processo utente che ha generato il page fault 
    state_t     *excState; //punt allo stato della CPU al momento della page fault
    unsigned int excCode;
    unsigned int vpn;
    int          p;
    int          frame;
    memaddr      frameAddr;
    int          status;

    supp     = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); // ottengo la support struct del processo che genera pagafault
    excState = &supp->sup_exceptState[PGFAULTEXCEPT]; // stato della CPU al momento della page fault
    excCode  = excState->cause & CAUSE_EXCCODE_MASK;         // estrae il codice di eccezione

    if (excCode == EXC_TLBMOD) { // se tentativo w in una pagina segnata come "solo lettura"*/
        supportProgramTrapHandler(supp); // program trap
        return;
    }

    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0); // acquiszione semaforo del swap pool 

    
    vpn = excState->entry_hi >> VPNSHIFT;
    p   = pageIndexFromVPN(vpn); /* passo 5 */

    frame     = pickFrame(); /* passo 6 */
    frameAddr = SWAPPOOLSTART + (frame * PAGESIZE); //calcolo l'indirizzo fisico del frame scelto

    if (swapPoolTable[frame].sw_asid != -1) { // frame occupato
        int         victimAsid = swapPoolTable[frame].sw_asid;
        int         victimPage = swapPoolTable[frame].sw_pageNo;
        pteEntry_t *victimPte  = swapPoolTable[frame].sw_pte; 

        /* 8a+8b: invalidazione PTE e aggiornamento TLB atomici (sez. 10:
         * TLBP+TLBWI mirati sulla sola entry della vittima, invece di
         * TLBCLR() . */
        setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); //disabilito interrupt globali
        victimPte->pte_entryLO &= ~VALIDON; //spengo il bit di validità della PTE del processo vittima
        tlbUpdate(victimPte->pte_entryHI, victimPte->pte_entryLO); //aggiorno la TLB con il nuovo valore della PTE del processo vittima.
        setSTATUS(getSTATUS() | MSTATUS_MIE_MASK); //riabilito gli interrupt globali.
        status = flashOperation(victimAsid, victimPage, frameAddr, TRUE); 
        if (status != READY) { // controlla buona riuscita della scrittura su flash.
            SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
            supportProgramTrapHandler(supp); 
            return;
        }
    }

    status = flashOperation(supp->sup_asid, p, frameAddr, FALSE); 
    if (status != READY) {
        SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
        supportProgramTrapHandler(supp);
        return;
    }

    /* aggiorno la swap pool */
    swapPoolTable[frame].sw_asid   = supp->sup_asid;
    swapPoolTable[frame].sw_pageNo = p;
    swapPoolTable[frame].sw_pte    = &supp->sup_privatePgTbl[p]; // il pager riconoscere quale pte invalidare

    /* aggiornamento PTE corrente e TLB atomici. */
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK); //disabilito gli interrupt globali
    supp->sup_privatePgTbl[p].pte_entryLO = (frameAddr & 0xFFFFF000) | DIRTYON | VALIDON; //imposto l'indirizzo fisico del frame appena caricato, accendo bit di validità e di scrittura.
    tlbUpdate(supp->sup_privatePgTbl[p].pte_entryHI, supp->sup_privatePgTbl[p].pte_entryLO); //aggiorno la TLB con il nuovo valore della PTE
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);//riabilita gli interrupt

    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);

    LDST(excState);
}
