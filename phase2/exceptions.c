#include "./headers/exceptions.h";

/* Private Check methods */
bool __CAUSE_IS_TLB__(unsigned int causeCode)
{
    return (causeCode <= EXC_MOD && causeCode >= EXC_UTLBS);
}

bool __CAUSE_IS_SYSCALL__(unsigned int causeCode)
{
    return (causeCode == EXC_ECU || causeCode == EXC_ECM);
}

bool __CAUSE_IS_TRAP__(unsigned int causeCode)
{
    return (
        (causeCode <= EXC_IAM && EXC_SAF) || causeCode == EXC_ECS || causeCode == 10 || (causeCode <= EXC_IPF && causeCode >= 23));
}

void uTLB_RefillHandler()
{
    // This code was provided in ./p2test.c
    // It has to be replaced in phase3
    prid_t prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)BIOSDATAPAGE);
}

/* Helper functions*/
pcb_t *getRoot(pcb_t *current)
{
    if (current->p_parent == NULL)
    {
        return current;
    }
    else
    {
        return getRoot(current->p_parent);
    }
}

pcb_t *findByPid(pcb_t *root, int pid)
{
    if (root == NULL)
    {
        return NULL;
    }
    else if (root->p_pid == pid)
    {
        return root;
    }
    else
    {
        struct list_head *iter;
        list_for_each(iter, &root->p_child)
        {
            pcb_t *child = container_of(iter, pcb_t, p_sib);
            pcb_t *found = findByPid(child, pid);

            // Se trovato nel sotto-albero, lo propaghiamo verso l'alto
            if (found != NULL)
            {
                return found;
            }
        }
        return NULL;
    }
}

void killProgeny(pcb_t *term)
{
    // Libero tutti i discendenti ricorsivamente
    while (!emptyChild(term))
    {
        pcb_t *child = removeChild(term);
        killProgeny(child);
    }

    if (term->p_semAdd != NULL)
    {
        // bloccato in attesa di un sem
        outBlocked(term);
        // TODO: Ricerca sui semafori dei device, in caso decremento
        // soft_block_counter--;
    }
    else if (term != current_process)
    {
        // current_process e' il processo in esecuzione, essendo in un'arch
        // monocore, assumo che se non e' current process e non e' blocccato
        // da un semaforo, allora e' in ready queue
        outProcQ(&ready_queue, term);
    }

    process_counter--;
    freePcb(term);
}

/* SYSCALL */

// Create Process
void NSYS1(state_t *excState)
{

    state_t *newState = (state_t *)excState->reg_a1;
    int prio = (int)excState->reg_a2;
    support_t *supportLevel = (support_t *)excState->reg_a3;
    pcb_t *newPcb = allocPcb();

    if (newPcb == NULL)
    {
        excState->reg_a0 = CREATEPROCESS;
    }
    else
    {
        newPcb->p_s = *newState;
        newPcb->p_prio = prio;
        newPcb->p_supportStruct = supportLevel;
        newPcb->p_time = 0;
        newPcb->p_semAdd = NULL;
        insertProcQ(&ready_queue, newPcb);
        insertChild(current_process, newPcb);
        process_counter++;
        excState->reg_a0 = (int)newPcb->p_pid;
    }
}

// Terminate Process
void NSYS2(state_t *excState)
{
    int pid = (int)excState->reg_a1;

    pcb_t *term = NULL;

    if (pid == 0)
    {
        term = current_process;
    }
    else
    {
        // Cerco il pcb partendo dalla radice (init)
        term = findByPid(getRoot(current_process), pid);
    }
    // terminazione processo
    if (term != NULL)
    {
        killProgeny(term);
        outChild(term);
    }
    scheduler();
}

// Passeren
void NSYS3(state_t *excState)
{
    int *semAdd = (int *)excState->reg_a1;
    if (*semAdd <= 0)
    {
        // Istruzioni per SYSCALL bloccanti
        excState->pc_epc += 4;
        updateCPUTime();
        current_process->p_s = *excState;

        insertBlocked(semAdd, current_process);
        scheduler();
    }
    else
    {
        (*semAdd)--;
        excState->pc_epc += 4;
    }
}

// Verhogen
void NSYS4(state_t *excState)
{
    int *semAdd = (int *)excState->reg_a1;
    if (headBlocked(semAdd) == NULL)
    {
        (*semAdd)++;
    }
    else
    {
        current_process = removeBlocked(semAdd);
    }
}

// DoIO
void NSYS5(state_t *excState)
{
    // in reg_a1 è presente l'indice che punta al semaforo su cui current proc si blocca
    int semIndex = (int)excState->reg_a1;
    excState->reg_a1 = device_semaphores[semIndex];
    NSYS3(excState);
}

// GetCPUTime
void NSYS6(state_t *excState);

// WaitForClock
void NSYS7(state_t *excState);

// GetSupportData
void NSYS8(state_t *excState);

// GetProcessID
void NSYS9(state_t *excState);

// Yield
void NSYS10(state_t *excState);

void syscall_exception_handler(state_t *excState)
{
    unsigned int previousMode = (excState->status & MSTATUS_MPP_MASK);

    // Simula un Program Trap per istruzione privilegiata
    if (excState->reg_a0 < 0 && previousMode != MSTATUS_MPP_M)
    {
        excState->cause = PRIVINSTR;
        trap_exception_handler(excState); // Passa il controllo al gestore dei Trap
        return;
    }
    // Controllo istruzione in kernel mode
    if (excState->reg_a0 < 0 && previousMode == MSTATUS_MPP_M)
    {
        // Avanzo nel program counter di una word esplicitamente
        excState->pc_epc += 4;

        switch (excState->reg_a0)
        {
        case CREATEPROCESS:
            NSYS1(excState);
            break;

        case TERMPROCESS:
            NSYS2(excState);
            break;

        case PASSEREN:
            NSYS3(excState);
            break;

        case VERHOGEN:
            NSYS4(excState);
            break;

        case DOIO:
            NSYS5(excState);
            break;

        case GETTIME:
            NSYS6(excState);
            break;

        case CLOCKWAIT:
            NSYS7(excState);
            break;

        case GETSUPPORTPTR:
            NSYS8(excState);
            break;

        case GETPROCESSID:
            NSYS9(excState);
            break;

        case YIELD:
            NSYS10(excState);
            break;
        }

        int retValue = SYSCALL(CREATEPROCESS, excState->reg_a1, excState->reg_a2, excState->reg_a3);
    }
}

void trap_exception_handler();

void passUpOrDie(int except_index)
{
    if (current_process->p_supportStruct == NULL)
        NSYS2(current_process->p_s);
    else
    {
        state_t *bios_state = (state_t *)GET_EXCEPTION_STATE_PTR(0);
        current_process->p_supportStruct->sup_exceptState[except_index] = *bios_state;

        unsigned int sp = current_process->p_supportStruct->sup_exceptContext[except_index].stackPtr;
        unsigned int status = current_process->p_supportStruct->sup_exceptContext[except_index].status;
        unsigned int pc = current_process->p_supportStruct->sup_exceptContext[except_index].pc;
        LDCXT(sp, status, pc);
    }
}

void exception_handler()
{
    // Lo stato di eccezione del processore, al momento dell'eccezione, viene salvato all'indirizzo BIOSDATAPAGE
    state_t *excState = GET_EXCEPTION_STATE_PTR(process_counter);

    unsigned int excCause = excState->cause;
    unsigned int excStatus = excState->status;

    // Estraggo il codice dell'eccezione e della modalita' usando le maschere
    unsigned int excCode = (excCause & CAUSE_EXCCODE_MASK);

    // 8.1, 8.2
    if (current_process->p_s.reg_a0 > 0 || __CAUSE_IS_TRAP__(excCode))
    {
        passUpOrDie(GENERALEXCEPT);
        return;
    }
    else if (excCause >= 24 && excCause <= 28) // 8.3
    {
        passUpOrDie(PGFAULTEXCEPT);
        return;
    }

    unsigned int previousMode = excStatus & MSTATUS_MPP_MASK;

    if (CAUSE_IS_INT(excCause))
        handleInterrupt();

    /*
else if (__CAUSE_IS_TLB__(excCode))
    tlb_exception_handler();
*/
    else if (__CAUSE_IS_SYSCALL__(excCode))
        syscall_exception_handler(excState);
    /*
        else if (__CAUSE_IS_TRAP__(excCode))
            trap_exception_handler();
    */
    else
        // codice eccezione non riconosciuto
        return;
}
