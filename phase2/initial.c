#include "./headers/initial.h"

// Global Variables 3 level
int process_counter;
int soft_block_counter; // waiting process
struct list_head ready_queue;
pcb_t *current_process;
int device_semaphores[SEMDEVLEN];
extern void uTLB_RefillHandler(void);

void* memcpy(void *dest, const void *src, unsigned int len)
{
    char *d = dest;
    const char* s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}


void populate_puv(passupvector_t *puv)
{                                                         
    puv->tlb_refill_handler = (memaddr)uTLB_RefillHandler; 
}

void set_sp_tlb_refill(passupvector_t *puv)
{                                           
    puv->tlb_refill_stackPtr = KERNELSTACK; 
}

void connect_exception_handler(passupvector_t *puv)
{
    puv->exception_handler = (memaddr)exception_handler;
    puv->exception_stackPtr = KERNELSTACK;
}

void init_data_structures()
{
    initPcbs();
    initASL();

}

void init_global_vars()
{ 
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
    devregarea->intervaltimer = LDIT(PSECOND);
}

extern void test();

void init_new_proc(void)
{
    pcb_t *p = allocPcb();
    memaddr ramtop; 
    if (p != NULL)
    {
        p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // imposto il processo in kernel mode
        p->p_s.mie = MIE_ALL;                              // abilita le interrupt per il processo
        p->p_s.pc_epc = (memaddr)test;
        RAMTOP(ramtop);         // ottengo l'indirizzo della cima della memoria RAM
        p->p_s.reg_sp = ramtop; // imposto lo stack pointer del processo alla cima della memoria RAM
        p->p_time = 0;
        p->p_semAdd = NULL;
        p->p_supportStruct = NULL;
        insertProcQ(&ready_queue, p);
        process_counter++;
    }
}

int main()
{
    passupvector_t *puv = (passupvector_t *)PASSUPVECTOR;
    devregarea_t *devregarea = (devregarea_t *)RAMBASEADDR; 

    populate_puv(puv);
    set_sp_tlb_refill(puv);
    connect_exception_handler(puv);
    init_data_structures();
    init_global_vars();
    load_interval_time(devregarea);
    init_new_proc();
    scheduler();

    return 0;
}