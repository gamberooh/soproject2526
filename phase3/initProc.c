#include "./headers/initProc.h"
#include "./headers/vmSupport.h"
#include "./headers/sysSupport.h"
#include <uriscv/liburiscv.h>

/* Semafori globali del Support Level (sez. 9 delle specifiche) */
int masterSemaphore; // definisce l'accesso mutualmente esclusivo per il processo test (master)
int shellSemaphore; // definisce l'interscambio tra shell e programmi in esecuzione
int flashMutex[UPROCMAX]; 
/* gestione mutex dei terminali in r/w */
int termReadMutex; 
int termWriteMutex;

static support_t        suppStruct_table[UPROCMAX];
static struct list_head suppStructFree_h;  
static int              suppStructSem; 

void initSupportStructs(void) {
    INIT_LIST_HEAD(&suppStructFree_h);
    for (int i = 0; i < UPROCMAX; i++) {
        list_add(&suppStruct_table[i].s_list, &suppStructFree_h);
    }
    suppStructSem = 1; // permetto l'accesso agli uproc
}

support_t *allocSupportStruct(void) {
    support_t *s;
    SYSCALL(PASSEREN, (int)&suppStructSem, 0, 0); // garantisco mutex sulla support struct
    if (list_empty(&suppStructFree_h)) {
        s = NULL;
    } else {
        struct list_head *first = suppStructFree_h.next; 
        list_del(first); // rimozione per evitare conflitti
        s = container_of(first, support_t, s_list);
    }
    SYSCALL(VERHOGEN, (int)&suppStructSem, 0, 0);
    return s;
}

void freeSupportStruct(support_t *s) {
    SYSCALL(PASSEREN, (int)&suppStructSem, 0, 0);
    list_add_tail(&s->s_list, &suppStructFree_h);
    SYSCALL(VERHOGEN, (int)&suppStructSem, 0, 0);
}

static void initDeviceMutex(void) {
    for (int i = 0; i < UPROCMAX; i++) {
        flashMutex[i] = 1;
    }
    termReadMutex  = 1;
    termWriteMutex = 1;
}

void initUprocState(state_t *s, int asid) { 
    /* Azzera TUTTI i registri generali */
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        s->gpr[i] = 0; 
    }
    s->cause    = 0; 
    s->pc_epc   = UPROCSTARTADDR;
    s->reg_sp   = USERSTACKTOP; 
    s->status   = MSTATUS_MIE_MASK;    // interrupt globali on; MPP resta 0 (user mode)
    s->mie      = MIE_ALL;             // interrupt disabilitati
    s->entry_hi = (asid << ASIDSHIFT); // ASID del processo utente.
}

void initUprocSupport(support_t *supp, int asid) {
    supp->sup_asid = asid;

    //configurazione per l'eccezioni di tipo pagefault
    supp->sup_exceptContext[PGFAULTEXCEPT].pc       = (memaddr)pager;                       // pager gestisce page faults
    supp->sup_exceptContext[PGFAULTEXCEPT].status   = MSTATUS_MIE_MASK | MSTATUS_MPP_M;     // interrupt globale + kernel mode
    supp->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)&(supp->sup_stackTLB[499]);  // salvo lo stato del processore nello stack pointer fornito dalla supp struct

    supp->sup_exceptContext[GENERALEXCEPT].pc       = (memaddr)supportGeneralExceptionHandler;
    supp->sup_exceptContext[GENERALEXCEPT].status   = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    supp->sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)&(supp->sup_stackGen[499]);

    unsigned int TEXT_DATA = USERPGTBLSIZE - 1; // Area text & data
    for (int i = 0; i < TEXT_DATA; i++) { //per ogni pagina della tabella delle pagine private del processo utente, tranne l'ultima che e' riservata allo stack, imposto il campo entryHI e entryLO della PTE. L'entryHI contiene l'indirizzo virtuale della pagina e l'ASID del processo, mentre l'entryLO contiene il flag DIRTYON per indicare che la pagina e' scrivibile.
        supp->sup_privatePgTbl[i].pte_entryHI = (KUSEG + (i * PAGESIZE)) | (asid << ASIDSHIFT);// . L'indirizzo virtuale della pagina viene calcolato come KUSEG + i*PAGESIZE, dove KUSEG è l'indirizzo base dello spazio utente e PAGESIZE è la dimensione di una pagina. L'ASID viene shiftato a sinistra di ASIDSHIFT bit per essere posizionato correttamente nel campo entryHI.
        supp->sup_privatePgTbl[i].pte_entryLO = DIRTYON;  // imposto il campo entryLO della tabella delle pagine private del processo utente, che contiene il flag DIRTYON per indicare che la pagina e' scrivibile. In questo modo, il processore sa che può scrivere su questa pagina senza generare un page fault.
    }
    // stack page
    unsigned int STACK_PAGE_IDX = USERPGTBLSIZE - 1;
    supp->sup_privatePgTbl[STACK_PAGE_IDX].pte_entryHI = (USERSTACKTOP - PAGESIZE) | (asid << ASIDSHIFT); //userstacktop - pagesize = 0xBFFFF000, che è l'indirizzo virtuale della pagina di stack del processo utente. 
    supp->sup_privatePgTbl[STACK_PAGE_IDX].pte_entryLO = DIRTYON;
}

 
void test(void) {
    state_t     shellState;
    support_t  *shellSupp;

    initSupportStructs(); //inizializza la lista di support_t liberi
    initDeviceMutex(); //inizializza i mutex dei dispositivi
    initSwapStructs(); //inizializza le strutture per la gestione dello swap, impostando tutti i frame della swap area come liberi (0) e tutti i frame della RAM come liberi (0)

    masterSemaphore = 0; //serve a far aspettare il processo master (test()) finche non termina la shell
    shellSemaphore  = 0; //serve a far aspettare la shell quando lancia un programma con essa(finche non termina il programma). quando il programma termina, viene fatta una V() sul semaforo della shell, che quindi riprende l'esecuzione
 
    shellSupp = allocSupportStruct();// alloca un support_t dalla lista suppStructFree_h, rimuovendolo dalla lista e restituendolo. se la lista e' vuota, restituisce NULL
    initUprocSupport(shellSupp, 1); // inizializza la support structure per il processo shell, impostando i campi sup_asid, sup_exceptContext e sup_privatePgTbl. 1 è l'ASID del processo shell 
    initUprocState(&shellState, 1); 

    SYSCALL(CREATEPROCESS, (int)&shellState, PROCESS_PRIO_LOW, (int)shellSupp); //crea il processo shell, passando lo stato del processo shell, la priorità del processo shell e la support structure del processo shell. Il sistema operativo crea il processo shell e lo mette nella coda dei processi pronti.

    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0); //  fa aspettare il processo master (test()) finche non termina la shell. Quando la shell termina, viene fatta una V() sul semaforo masterSemaphore, che quindi riprende l'esecuzione del processo master (test()).

    SYSCALL(TERMPROCESS, 0, 0, 0); // termina il processo master (test()), che quindi non puo' piu' eseguire alcuna istruzione. Il sistema operativo libera tutte le risorse allocate al processo master (test()) e lo rimuove dalla coda dei processi pronti.
}
