#include "./headers/initial.h"

// Global Variables 3 level
int process_counter;
int soft_block_counter; // waiting process
struct list_head ready_queue;
pcb_t *current_process;
int device_semaphores[SEMDEVLEN];
void populate_puv(passupvector_t *puv)
{                                                            // se la cpu non trova un indirizzo in memoria (TLB Refill), va a leggere questa istruzione d
    puv->tlb_refill_handler = (memaddr)uTLB_RefillHandler(); // tlb_refill_events_handler(); // puv è un puntatore alla passupvector, tlb_refill_handler è un campo della passupvector che contiene l'indirizzo della funzione da chiamare in caso di TLB Refill, uTLB_RefillHandler è la funzione che gestisce il TLB Refill
}

void set_sp_tlb_refill(passupvector_t *puv)
{                                           // funzione che imposta lo stack pointer per il TLB Refill, in modo che quando viene chiamata la funzione di gestione del TLB Refill, lo stack pointer punti alla cima dello stack del kernel (KERNELSTACK)
    puv->tlb_refill_stackPtr = KERNELSTACK; // ho modificato al posto di tlb_refill_handler, tlb_refill_stackPtr perche KERNELSTACK è un indirizzo di memoria quindi va il campo stackPtr e non handler(che serve per le istruzioni)
}

void connect_exception_handler(passupvector_t *puv)
{ // funzione che gestisce le altre emergenze(crash, system call, ecc). Quando viene chiamata una di queste emergenze, la CPU va a leggere l'indirizzo della funzione da chiamare in caso di emergenza (exception_handler) e lo stack pointer da usare (exception_stackPtr) dalla passupvector, e chiama la funzione di gestione dell'emergenza (exceptionHandler) con lo stack pointer impostato a KERNELSTACK
    puv->exception_handler = (memaddr)exception_handler();
    puv->exception_stackPtr = KERNELSTACK;
}

void init_data_structures()
{ // chiama le funzoni di inizializzazione dei pcb e asl
    initPcbs();
    initASL();
}

void init_global_vars()
{ // mette a 0 i contatori dei processi e dei processi in attesa, inizializza la ready queue e i semafori dei dispositivi
    process_counter = 0;
    soft_block_counter = 0; // waiting process
    mkEmptyProcQ(&ready_queue);
    current_process = NULL;
    for (int i = 0; i < SEMDEVLEN; i++)
    {
        device_semaphores[i] = 0; // init = 0 to garantee mutex
    }
}

void load_interval_time(devregarea_t *devregarea)
{ // imposta il timer a 100ms
    devregarea->intervaltimer = PSECOND;
}

extern void test();

void init_new_proc(void)
{
    pcb_t *p = allocPcb();
    memaddr ramtop; // variabile che conterrà l'indirizzo della cima della memoria RAM, che sarà usata come stack pointer per il nuovo processo
                    // ho trovato memaddr in types.h, rappresenta un indirizzo di memoria
    if (p != NULL)
    {
        p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // imposto il processo in kernel mode
        p->p_s.mie = MIE_ALL;                              // abilita le interrupt per il processo
        p->p_s.pc_epc = (memaddr)test;
        // tutti i campi dopo la freccia sono campi della struttura state_t, sempre nel file types.h, che rappresenta lo stato del processo, e che contiene il program counter (pc_epc), il registro di stato (status) e il registro delle interrupt (mie)
        RAMTOP(ramtop);         // ottengo l'indirizzo della cima della memoria RAM
        p->p_s.reg_sp = ramtop; // imposto lo stack pointer del processo alla cima della memoria RAM

        insertProcQ(&ready_queue, p);
        process_counter++;
    }
}

int main()
{
    passupvector_t *puv = (passupvector_t *)PASSUPVECTOR;   // al posto di passupvector_t *puv che è una dichiarazione di una variabile locale, ho scritto in questo modo così che la cpu possa leggere l'indirizzo della passupvector dalla memoria (PASSUPVECTOR)
    devregarea_t *devregarea = (devregarea_t *)RAMBASEADDR; // stesso discorso di prima, la cpu legge l'indirizzo del devregarea dalla memoria (RAMBASEADDR)

    populate_puv(puv); // ho rimosso la & davanti a puv perche puv è già un puntatore, quindi non serve prendere l'indirizzo di puv
    set_sp_tlb_refill(puv);
    connect_exception_handler(puv); // ho modificato al posto di connect_exption_handler, connect_exception_handler (typo)
    init_data_structures();
    init_global_vars();
    load_interval_time(devregarea);
    init_new_proc();
    scheduler();

    return 0;
}