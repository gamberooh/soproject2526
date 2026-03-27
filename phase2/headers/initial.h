#include "../../phase1/headers/asl.h";
#include "../../phase1/headers/pcb.h";
#include <uriscv/types.h>
extern pcb_t *current_process;
extern struct list_head ready_queue;
extern int process_counter;
extern int soft_block_counter;
// ho aggiunto tutte queste con extern in modo da poterle usare in tutti i file del progetto, altrimenti non potevo accedere a queste variabili da altri file(come scheduler.c ad esempio) e mi dava errore di variabile non definita

void populate_puv(passupvector_t *puv);

void set_sp_tlb_refill(passupvector_t *puv);

void connect_exception_handler(passupvector_t *puv);

void init_data_structures();

void init_global_vars();

void load_interval_time(devregarea_t *devregarea);

void init_new_proc();

void scheduler();