#include "../../phase1/headers/asl.h";
#include "../../phase1/headers/pcb.h";
#include <uriscv/types.h>

// #define KEYBOARD 0;


void populate_pu_vect(passupvector_t* puv);

void set_sp_tlb_refill(passupvector_t* puv);

void connect_exception_handler(passupvector_t* puv);

void init_data_structures();

void init_global_var();

void load_interval_time();

void init_new_proc();

void init_scheduler();