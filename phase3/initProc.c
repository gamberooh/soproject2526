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
    for (int i = 0; i < TEXT_DATA; i++) { 
        supp->sup_privatePgTbl[i].pte_entryHI = (KUSEG + (i * PAGESIZE)) | (asid << ASIDSHIFT);
        supp->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
    }
    // stack page
    unsigned int STACK_PAGE_IDX = USERPGTBLSIZE - 1;
    supp->sup_privatePgTbl[STACK_PAGE_IDX].pte_entryHI = (USERSTACKTOP - PAGESIZE) | (asid << ASIDSHIFT); //userstacktop - pagesize = 0xBFFFF000 
    supp->sup_privatePgTbl[STACK_PAGE_IDX].pte_entryLO = DIRTYON;
}

 
void test(void) {
    state_t     shellState;
    support_t  *shellSupp;

    initSupportStructs(); 
    initDeviceMutex(); 
    initSwapStructs(); // frame swap pool & frame RAM = 0

    masterSemaphore = 0; // test rimane in waiting finché shell non rilascia la risorsa.
    shellSemaphore  = 0; // shell rimane in waiting finché un programma di test non termina.
    int SHELL_ASID = 1;
    shellSupp = allocSupportStruct();
    initUprocSupport(shellSupp, SHELL_ASID); 
    initUprocState(&shellState, SHELL_ASID); 

    SYSCALL(CREATEPROCESS, (int)&shellState, PROCESS_PRIO_LOW, (int)shellSupp); //crea il processo shell

    SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0); // test si mette in attesa della terminazione del processo shell

    SYSCALL(TERMPROCESS, 0, 0, 0);
}
