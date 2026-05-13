#include "initial.h"
#include <uriscv/types.h>

/* PASSUPORDIE */

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

void myTlbRefillHandler(void);

void syscall_exception_handler(state_t* mode);

void trap_exception_handler(state_t *excState);

void exception_handler(void);
