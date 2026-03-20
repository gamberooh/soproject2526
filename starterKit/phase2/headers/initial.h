#include "../../phase1/headers/asl.h";
#include "../../phase1/headers/pcb.h";
#include <uriscv/types.h>

void populate_puv(passupvector_t* puv);

void set_sp_tlb_refill(passupvector_t* puv);

void connect_exception_handler(passupvector_t* puv);

void init_data_structures();

void init_global_vars();

void load_interval_time(devregarea_t* devregarea);

void init_new_proc(pcb_t* newproc);

void init_scheduler();