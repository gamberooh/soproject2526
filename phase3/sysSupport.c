#include "./headers/sysSupport.h"
#include "./headers/initProc.h"
#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>

#define TERMDEVADDR(devNo) (START_DEVREG + 0x200 + ((devNo) * 0x10))

static void sysWriteTerminal(support_t *supp);
static void sysReadTerminal(support_t *supp);
static void sysExecute(support_t *supp);
static int  isValidUserAddr(memaddr addr);

/* terminazione ordinata del processo utente */
void terminateUproc(support_t *supp) {
    int asid = supp->sup_asid;
    freeUprocFrames(asid);
    freeSupportStruct(supp);

    /* invalida le entry tlb del processo */
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
    for (int i = 0; i < USERPGTBLSIZE; i++) {
        if (supp->sup_privatePgTbl[i].pte_entryLO & VALIDON) {
            tlbUpdate(supp->sup_privatePgTbl[i].pte_entryHI,
                      supp->sup_privatePgTbl[i].pte_entryLO & ~VALIDON);
        }
    }
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

    /* risveglia master o shell */
    if (asid == 1) {
        SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (int)&shellSemaphore, 0, 0);
    }

    SYSCALL(TERMPROCESS, 0, 0, 0);
}

/* trap di programma trattata come sys2 */
void supportProgramTrapHandler(support_t *supp) {
    terminateUproc(supp);
}

/* verifica indirizzo nello spazio utente */
static int isValidUserAddr(memaddr addr) {
    return (addr >= KUSEG) && (addr < USERSTACKTOP);
}

/* handler delle syscall di supporto */
static void supportSyscallHandler(support_t *supp) {
    state_t *state = &supp->sup_exceptState[GENERALEXCEPT];
    state->pc_epc += 4; /* avanza pc */

    switch ((int)state->reg_a0) {
    case TERMINATE:
        terminateUproc(supp);
        return;
    case WRITETERMINAL:
        sysWriteTerminal(supp);
        break;
    case READTERMINAL:
        sysReadTerminal(supp);
        break;
    case EXECUTE:
        sysExecute(supp);
        return;
    default:
        terminateUproc(supp);
        return;
    }

    LDST(state);
}

/* gestore generale delle eccezioni (syscall o trap) */
void supportGeneralExceptionHandler(void) {
    support_t   *supp = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = supp->sup_exceptState[GENERALEXCEPT].cause & CAUSE_EXCCODE_MASK;

    if (cause == EXC_ECU || cause == EXC_ECM) {
        supportSyscallHandler(supp);
    } else {
        supportProgramTrapHandler(supp);
    }
}

/* sys4: scrittura su terminale */
static void sysWriteTerminal(support_t *supp) {
    state_t   *state = &supp->sup_exceptState[GENERALEXCEPT];
    char      *vaddr = (char *)state->reg_a1;
    int        len   = (int)state->reg_a2;
    termreg_t *termReg;
    int        count;
    int        status;

    if (len < 0 || len > MAXSTRLENG || !isValidUserAddr((memaddr)vaddr) || !isValidUserAddr((memaddr)vaddr + len)) {
        terminateUproc(supp);
        return;
    }

    termReg = (termreg_t *)TERMDEVADDR(0);

    SYSCALL(PASSEREN, (int)&termWriteMutex, 0, 0);

    count  = 0;
    status = OKCHARTRANS;
    while (count < len && (status & 0xFF) == OKCHARTRANS) {
        status = SYSCALL(DOIO, (int)&termReg->transm_command, (vaddr[count] << 8) | TRANSMITCHAR, 0);
        if ((status & 0xFF) == OKCHARTRANS)
            count++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteMutex, 0, 0);

    state->reg_a0 = ((status & 0xFF) == OKCHARTRANS) ? count : -status;
}

/* sys5: lettura da terminale */
static void sysReadTerminal(support_t *supp) {
    state_t   *state = &supp->sup_exceptState[GENERALEXCEPT];
    char      *vaddr = (char *)state->reg_a1;
    termreg_t *termReg;
    int        count;
    int        status;
    char       ch = 0;

    if (!isValidUserAddr((memaddr)vaddr)) {
        terminateUproc(supp);
        return;
    }

    termReg = (termreg_t *)TERMDEVADDR(0);

    SYSCALL(PASSEREN, (int)&termReadMutex, 0, 0);

    count = 0;
    do {
        status = SYSCALL(DOIO, (int)&termReg->recv_command, RECEIVECHAR, 0);
        if ((status & 0xFF) == CHARRECV) {
            ch           = (char)((status >> 8) & 0xFF);
            vaddr[count] = ch;
            count++;
        }
    } while ((status & 0xFF) == CHARRECV && ch != '\n' && count < MAXSTRLENG);

    SYSCALL(VERHOGEN, (int)&termReadMutex, 0, 0);

    state->reg_a0 = ((status & 0xFF) == CHARRECV) ? count : -status;
}

/* sys6: esecuzione programma da parte della shell */
static void sysExecute(support_t *supp) {
    state_t    *state = &supp->sup_exceptState[GENERALEXCEPT];
    int         asid;
    state_t     newState;
    support_t  *newSupp;

    if (supp->sup_asid != 1) {
        terminateUproc(supp);
        return;
    }

    asid    = (int)state->reg_a1;
    newSupp = allocSupportStruct();
    if (newSupp == NULL) {
        LDST(state);
        return;
    }
    initUprocState(&newState, asid);
    initUprocSupport(newSupp, asid);

    SYSCALL(CREATEPROCESS, (int)&newState, PROCESS_PRIO_LOW, (int)newSupp);

    /* attesa terminazione del processo figlio */
    SYSCALL(PASSEREN, (int)&shellSemaphore, 0, 0);

    LDST(state);
}
