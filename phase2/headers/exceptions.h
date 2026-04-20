#include "initial.h"
#include <uriscv/types.h>

/**
 *  TLB, Program Trap, and SYSCALL exception handlers.
 *  Furthermore, this module will contain the provided
 *  skeleton TLB-Refill event handler
 *  (e.g. uTLB_RefillHandler).
 */

int __CAUSE_IS_TLB__(unsigned int cause);

int __CAUSE_IS_SYSCALL__(unsigned int cause);

int __CAUSE_IS_TRAP__(unsigned int cause);

/* Helper functions*/
pcb_t* getRoot(pcb_t* current);
pcb_t* findByPid(pcb_t* root, int pid);
void killProgeny(pcb_t* term);
void passUpOrDie(int except_index);

/* SYCALLs */

void NSYS1(state_t* excState);
void NSYS2(state_t* excState);
void NSYS3(state_t* excState);
void NSYS4(state_t* excState);
void NSYS5(state_t* excState);
void NSYS6(state_t* excState);
void NSYS7(state_t* excState);
void NSYS8(state_t* excState);
void NSYS9(state_t* excState);
void NSYS10(state_t* excState);

/* HANDLERS */

void tlb_exception_handler(void);

void syscall_exception_handler(state_t* mode);

void trap_exception_handler(state_t *excState);

void exception_handler(void);
