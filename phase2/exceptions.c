#include "./headers/exceptions.h";

// Private Check methods
bool __CAUSE_IS_TLB__(unsigned int cause) {
    return (cause <= EXC_MOD && cause >= EXC_UTLBS);
}

bool __CAUSE_IS_SYSCALL__(unsigned int cause) {
    return (cause == EXC_ECU || cause == EXC_ECM);
}

bool __CAUSE_IS_TRAP__(unsigned int cause) {
    return (
       ( cause <= EXC_IAM && EXC_SAF) 
    ||   cause == EXC_ECS 
    ||   cause == 10
    || ( cause <= EXC_IPF && cause >= 23) 
    );
}

void tlb_exception_handler() {
    // This code was provided in ./p2test.c
    // It has to be replaced in phase3
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) BIOSDATAPAGE);
}

void syscall_exception_handler(){
    
}

void trap_exception_handler();

void exception_handler() {
    // Lo stato di eccezione del processore, al momento dell'eccezione, viene salvato all'indirizzo BIOSDATAPAGE
    state_t* excstate = GET_EXCEPTION_STATE_PTR(process_counter);

    unsigned int exccause = excstate->cause;

    switch (exccause) {
    case CAUSE_IS_INT(exccause):
        handleInterrupt();
        break;
    case __CAUSE_IS_TLB__(exccause):
        tlb_exception_handler();
        break;
    case __CAUSE_IS_SYSCALL__(exccause):
        syscall_exception_handler();
        break;
    case __CAUSE_IS_TRAP__(exccause):
        trap_exception_handler();
        break;
    default:
        break;
    }
}