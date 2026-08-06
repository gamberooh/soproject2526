#include "./headers/initProc.h"
#include "./headers/vmSupport.h"
#include "./headers/sysSupport.h"
#include <uriscv/liburiscv.h>

/* Semafori globali del Support Level (sez. 9 delle specifiche) */
int masterSemaphore; //serve a far aspettare il processo master (test()) finche non termina la shell
int shellSemaphore;//serve a far aspettare la shell quando lancia un programma con essa(finche non termina il programma). quando il programma termina, viene fatta una V() sul semaforo della shell, che quindi riprende l'esecuzione
int flashMutex[UPROCMAX];// flashMutex[8]  serve a evitare che due processi scrivano sullo stesso flash contemporaneamente. Se un processo vuole scrivere su un flash, deve prima fare una P() sul semaforo flashMutex[flashNumber].
int termReadMutex; //  variabile globale che serve a evitare che due processi leggano dallo stesso terminale contemporaneamente. Se un processo vuole leggere da un terminale, deve prima fare una P() sul semaforo termReadMutex. 
int termWriteMutex;

/* Pool di Support Structure gestito come free-list, stesso pattern di phase1/pcb.c. */
static support_t       suppStruct_table[UPROCMAX];// structura di supporto per ogni processo utente, serve per gestire le eccezioni e la tabella delle pagine private 
static struct list_head suppStructFree_h;  //definisce la testa (il gancio d'inizio) di una lista concatenata bidirezionale di support_t, che contiene tutti i support_t liberi (non allocati a nessun processo utente).
static int              suppStructSem;//semaforo per l'accesso alla lista di support_t liberi, serve a evitare che due processi allocano lo stesso support_t contemporaneamente

void initSupportStructs(void) {
    INIT_LIST_HEAD(&suppStructFree_h);
    for (int i = 0; i < UPROCMAX; i++) {
        list_add(&suppStruct_table[i].s_list, &suppStructFree_h);
    }
    suppStructSem = 1; //dopo aver inizializzato la lista di support_t liberi, inizializzo il semaforo a 1 (libero) per permettere l'accesso alla lista da parte dei processi utente.
}
//funzione per allocare una support structure libera dalla lista di support_t liberi. 
support_t *allocSupportStruct(void) {
    support_t *s;
    SYSCALL(PASSEREN, (int)&suppStructSem, 0, 0); //faccio una P() sul semaforo per evitare che due processi allocano lo stesso support_t contemporaneamente. 
    if (list_empty(&suppStructFree_h)) {
        s = NULL;
    } else {
        struct list_head *first = suppStructFree_h.next; 
        list_del(first); //rimuove il primo elemento della lista di support_t liberi, in modo che non possa essere allocato da un altro processo.
        s = container_of(first, support_t, s_list); //assegnamo a s il puntatore al support_t allocato, usando la macro container_of per ottenere il puntatore al support_t a partire dal puntatore alla struct list_head.
    }
    SYSCALL(VERHOGEN, (int)&suppStructSem, 0, 0);
    return s;
}
//funzione che libera una support structure, aggiungendola alla lista di support_t liberi
void freeSupportStruct(support_t *s) {
    SYSCALL(PASSEREN, (int)&suppStructSem, 0, 0);
    list_add_tail(&s->s_list, &suppStructFree_h);
    SYSCALL(VERHOGEN, (int)&suppStructSem, 0, 0);
}

//funzione che inizializza i mutex dei dispositivi, impostando tutti i flashMutex a 1 (liberi) e i due termReadMutex/termWriteMutex a 1 (liberi)
static void initDeviceMutex(void) {
    for (int i = 0; i < UPROCMAX; i++) {
        flashMutex[i] = 1;
    }
    termReadMutex  = 1;
    termWriteMutex = 1;
}

void initUprocState(state_t *s, int asid) { //riceve in input un puntatore allo stato del processo utente da inizializzare e l'ASID del processo utente. 
    /* Azzera TUTTI i registri generali */
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        s->gpr[i] = 0; //gpr sono i registri generali del processore, che vengono azzerati per evitare che contengano valori casuali che potrebbero mandare in crash il processo utente alla prima istruzione
    }
    s->cause    = 0; //azzera il registro cause, che contiene la causa dell'ultima eccezione. 
    s->pc_epc   = UPROCSTARTADDR;//l'indirizzo di partenza del processo utente, che viene impostato a UPROCSTARTADDR come da specifiche di fase 3
    s->reg_sp   = USERSTACKTOP;//l'indirizzo di partenza dello stack del processo utente, che viene impostato a USERSTACKTOP (0xC0000000)
    s->status   = MSTATUS_MIE_MASK; /* interrupt globali on; MPP resta 0 (user mode) */
    s->mie      = MIE_ALL;          /* tutte le sorgenti interrupt abilitate, incluso il PLT */
    s->entry_hi = (asid << ASIDSHIFT); //metto mel registro entry_hi l'ASID del processo utente. quel numero viene shiftato a sinistra (<<)  
}
//funzione che prepara la support structure per un processo utente, inizializzando i campi sup_asid, sup_exceptContext e sup_privatePgTbl. prende in input un puntatore al support_t da inizializzare e l'ASID del processo utente

void initUprocSupport(support_t *supp, int asid) {
    supp->sup_asid = asid; //imposto l'ASID del processo utente nella support structure
    //configurazione per i pagefault
    //sup_exceptContext è un array di due context_t, uno per il page fault e uno per le eccezioni generali. Ogni context_t contiene i registri stackPtr, status e pc che vengono caricati nel processore quando si verifica l'eccezione corrispondente. 
    supp->sup_exceptContext[PGFAULTEXCEPT].pc       = (memaddr)pager; // se questo processo genera page fault, il processore salta all'indirizzo della funzione pager() che gestisce il page fault
    supp->sup_exceptContext[PGFAULTEXCEPT].status   = MSTATUS_MIE_MASK | MSTATUS_MPP_M; // abilito gli interrupt globali e imposto il livello di privilegio a kernel mode (M) per gestire il page fault. MSTATUS_MIE_MASK abilita gli interrupt globali e MSTATUS_MPP_M imposta il livello di privilegio a kernel mode (M)
    supp->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)&(supp->sup_stackTLB[499]);//do all'handler del pager uno stack pointer(preso dalla support structure) dove salvare lo stato del processore quando viene interrotto per gestire il page fault per non sporcare lo stack del processo utente.)

    supp->sup_exceptContext[GENERALEXCEPT].pc       = (memaddr)supportGeneralExceptionHandler; // se questo processo genera un'eccezione generale, il processore salta all'indirizzo della funzione supportGeneralExceptionHandler() che gestisce l'eccezione
    supp->sup_exceptContext[GENERALEXCEPT].status   = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    supp->sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)&(supp->sup_stackGen[499]);

    /* ogni processo ha una tabella di 32 pagine , le prime 31 per il codice(.text) e le variabili (.data), l'ultima per lo stack. */
    
    for (int i = 0; i < USERPGTBLSIZE - 1; i++) { //per ogni pagina della tabella delle pagine private del processo utente, tranne l'ultima che e' riservata allo stack, imposto il campo entryHI e entryLO della PTE. L'entryHI contiene l'indirizzo virtuale della pagina e l'ASID del processo, mentre l'entryLO contiene il flag DIRTYON per indicare che la pagina e' scrivibile.
        supp->sup_privatePgTbl[i].pte_entryHI = (KUSEG + (i * PAGESIZE)) | (asid << ASIDSHIFT);// . L'indirizzo virtuale della pagina viene calcolato come KUSEG + i*PAGESIZE, dove KUSEG è l'indirizzo base dello spazio utente e PAGESIZE è la dimensione di una pagina. L'ASID viene shiftato a sinistra di ASIDSHIFT bit per essere posizionato correttamente nel campo entryHI.
        supp->sup_privatePgTbl[i].pte_entryLO = DIRTYON;  // imposto il campo entryLO della tabella delle pagine private del processo utente, che contiene il flag DIRTYON per indicare che la pagina e' scrivibile. In questo modo, il processore sa che può scrivere su questa pagina senza generare un page fault.
    }
    /* Pagina di stack: VPN fisso a 0xBFFFF, ovvero l'indirizzo USERSTACKTOP - PAGESIZE */
    supp->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI = (USERSTACKTOP - PAGESIZE) | (asid << ASIDSHIFT); //userstacktop - pagesize = 0xBFFFF000, che è l'indirizzo virtuale della pagina di stack del processo utente. 
    supp->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
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
