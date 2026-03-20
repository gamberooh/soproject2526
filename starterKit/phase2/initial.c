#include "./headers/initial.h";
// Global Variables 3 level
int process_counter;
int soft_block_counter; // wainting process
static struct list_head ready_queue;
pcb_t* current_process;
unsigned int device_semaphores[SEMDEVLEN];

void populate_puv(passupvector_t* puv) {
    puv->tlb_refill_handler = (memaddr)uTLB_RefillHandler();
}

void set_sp_tlb_refill(passupvector_t* puv) {
    puv->tlb_refill_handler = KERNELSTACK;
}

void connect_exception_handler(passupvector_t* puv) {
    puv->exception_handler = (memaddr)exceptionHandler();
    puv->exception_stackPtr = KERNELSTACK;
}

void init_data_structures() {
    initxPCBs();
    initASL();
}

void init_global_vars() {
    process_counter = 0;
    soft_block_counter = 0; // wainting process
    mkEmptyProcQ(&ready_queue);
    current_process = NULL;
    for (int i = 0; i < SEMDEVLEN; i++) {
        device_semaphores[i] = 0; //init = 0 to garantee mutex
    }
}

void load_interval_time(devregarea_t* devregarea) {
    devregarea->intervaltimer = PSECOND;
}

void init_new_proc(pcb_t* newproc) {
    allocPcb(&newproc);
    state_t initState;
    initState.mie = MSTATUS_MPP_M;
    // Missing: stack pointer to RAMTOP
    newproc->p_s = initState;
    process_counter++;

    extern void test();
//debug
    newproc->p_s.pc_epc = (memaddr) test; 
}

//TODO
void init_scheduler();

int main() {
    passupvector_t puv;
    devregarea_t devregarea;
    pcb_t newproc;
    populate_puv(&puv);
    set_sp_tlb_refill(&puv);
    connect_exption_handler(&puv);
    init_data_structures();
    init_global_vars();
    load_interval_time(&devregarea);
    init_new_proc(&newproc);
    init_scheduler();

    return 0;
    
}