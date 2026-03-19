#include "./headers/initial.h";
// Global Variables 3 level
int process_counter;
int soft_block_counter; // wainting process
static struct list_head ready_queue;
pcb_t* current_process;
static struct semd_t device_semaphores[SEMDEVLEN];

void populate_pu_vect(passupvector_t* puv) {
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
    initPCBs();
    initASL();
}

void init_global_var() {
    process_counter = 0;
    soft_block_counter = 0; // wainting process
    mkEmptyProcQ(&ready_queue);
    current_process = NULL;
    for (int i = 0; i < SEMDEVLEN; i++) {
        INIT_LIST_HEAD(&device_semaphores[i]);
    }
}

// void load_interval_time();

// void init_new_proc();

// void init_scheduler();

int main() {
    passupvector_t* puv;

    return 0;
    
}