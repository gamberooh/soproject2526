#include "initial.h";

/**
 *  TLB, Program Trap, and SYSCALL exception handlers.
 *  Furthermore, this module will contain the provided
 *  skeleton TLB-Refill event handler
 *  (e.g. uTLB_RefillHandler).
 */

bool __CAUSE_IS_TLB__(unsigned int cause);

bool __CAUSE_IS_SYSCALL__(unsigned int cause);

bool __CAUSE_IS_TRAP__(unsigned int cause);

void tlb_exception_handler();

void syscall_exception_handler();

void trap_exception_handler();

void exception_handler();
