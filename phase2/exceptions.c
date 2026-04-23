#include "./headers/exceptions.h"
#include <uriscv/types.h>

/* Private Check methods */
int __CAUSE_IS_TLB__(unsigned int causeCode)
{
    return (causeCode <= EXC_MOD && causeCode >= EXC_UTLBS);
}

int __CAUSE_IS_SYSCALL__(unsigned int causeCode)
{
    return (causeCode == EXC_ECU || causeCode == EXC_ECM);
}

int __CAUSE_IS_TRAP__(unsigned int causeCode)
{
    return (
        (causeCode <= EXC_IAM && causeCode >= EXC_SAF) || causeCode == EXC_ECS || causeCode == 10 || (causeCode <= EXC_IPF && causeCode >= 23));
}

void myTlbRefillHandler()
{
    // This code was provided in ./p2test.c
    // It has to be replaced in phase3
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)BIOSDATAPAGE);
}

/* Helper functions*/

int isDeviceSemaphore(int *semAddr)
{
    return (semAddr >= &device_semaphores[0]) && (semAddr < &device_semaphores[SEMDEVLEN]);
}

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
        int *semAddr = term->p_semAdd; // salvo prima l'indirizzo del semaforo qui, perche outBlocked mette a NULL il p_semAdd del processo term. mi serve dunque per il controllo di isDeviceSemaphore dopo outBlocked.
        // bloccato in attesa di un sem
        outBlocked(term);
        if (isDeviceSemaphore(semAddr))
            soft_block_counter--;
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
    pcb_t *newPcb = NULL;
    memcpy(newPcb, allocPcb(), sizeof(pcb_t));

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
        // stacco il processo term dall'albero dei processi creando un'isola che non ha
        // dipendenze da processi attivi.
        // cosicché
        outChild(term);
        killProgeny(term);
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
        current_process->p_s = *excState;

        if (insertBlocked(semAdd, current_process))
        {
            // se ci dovesse essere un errore nell'inserimento del processo
            // nella ASL, allora faccio pass up al processo padre
            passUpOrDie(GENERALEXCEPT);
            return;
        }
        // Se semAdd fa parte dei descrittori dei semafori di device
        // dobbiamo incrementare il soft_block_counter da spec.
        if (isDeviceSemaphore(semAdd))
        {
            soft_block_counter++;
        }
        scheduler();
    }
    else
    {
        // Non è bloccante
        (*semAdd)--; 
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
        insertProcQ(&ready_queue, removeBlocked(semAdd)); // il processo appena liberato, va in ready queue
        if (isDeviceSemaphore(semAdd))
        {
            soft_block_counter--;
        }
    }
}

// DoIO
void NSYS5(state_t *excState)
{
    memaddr commandAddr = (memaddr)excState->reg_a1;
    // Ottengo l'indice del device
    // - commandAddr -> indirizzo da cui arriva il comando
    // - START_DEVREG -> indirizzo di partenza dal quale iniziano le aree contigue in cui sono salvati i devices
    // - commandAddr - START_DEVREG = ottengo un offset da cui posso ottenere l'indice del device, dividendolo
    //                                per lo spazio di memoria occupato da un device.
    // ogni device è grande 16 bit
    int *indirizzoa1 = (int *)excState->reg_a1;
    int indirizzoa_2 = (int)excState->reg_a2;
    *indirizzoa1 = indirizzoa_2;

    int devIndex = (commandAddr - START_DEVREG) / 0x10;
    int offset = (commandAddr - START_DEVREG) % 0x10;

    int semIndex;
    if (devIndex >= 32)
    {
        if (offset < 8)
        {
            semIndex = SEM_TERM_RX_0 + (devIndex - 32);
        }
        else
        {
            semIndex = SEM_TERM_TX_0 + (devIndex - 32);
        }
    }
    else
    {
        // +1 giustificato dalla scelta di mettere pseudoclock per primo (0)
        semIndex = devIndex + 1;
    }
    excState->reg_a1 = (memaddr) device_semaphores[semIndex];
    NSYS3(excState); // faccio la p sul semaforo indicato dal cont. del registro a1
    // soft_block_counter++;

    // if (IS_TERMINAL(devIndex))
    /* {
         if(IS_TERMINAL_RX(devIndex)) {

         }

         else if (IS_TERMINAL_TX(devIndex)) {

         }

     }*/

    // Quando il sub device (del terminal) lancia un interrupt, il nucleo fa una V() sul sotto-device dedicato
}

// (a0 -> nSyscall, a1 -> reg generale, a2, a3)
// GetCPUTime
void NSYS6(state_t *excState)
{
    excState->reg_a0 = (unsigned int)current_process->p_time; // dato che ho aggiornato il tempo di CPU in updateCPUTime all'inizio di exception_handler, posso semplicemente restituire il tempo di CPU del processo corrente sneza fare calcoli aggiuntivi.
};

// WaitForClock
void NSYS7(state_t *excState)
{
    int *semAdd = &device_semaphores[SEM_PSEUDOCLOCK]; // SEM_PSEUDOCLOCK è l'indice del semaforo associato al clock(100ms)
    current_process->p_s = *excState;                  // salvo lo stato del processo prima di bloccarlo
    if (insertBlocked(semAdd, current_process))
    {
        passUpOrDie(GENERALEXCEPT);
        return;
    } // insertBlocked da false se l'inserimento va bene, True se ce errore (es ASL piena)
    soft_block_counter++;
    scheduler();
};

// GetSupportData
void NSYS8(state_t *excState)
{
    // prendo il valore del support struct pointer del processo corrente e lo restituisco nel registro a0. se il puntatore è NULL, restituisco 0
    excState->reg_a0 = (unsigned int)current_process->p_supportStruct;
};

// GetProcessID
void NSYS9(state_t *excState)
{
    // se il parametro a1 è 0, restituisco il pid del processo corrente, altrimenti restituisco il pid del padre
    int parent = (int)excState->reg_a1;
    if (parent == 0)
    {
        excState->reg_a0 = current_process->p_pid;
    }
    else
    {
        if (current_process->p_parent == NULL) // caso in cui il processo corrente sia il processo root
        {
            excState->reg_a0 = 0;
        }
        else
        {
            excState->reg_a0 = current_process->p_parent->p_pid;
        }
    }
};

// Yield
void NSYS10(state_t *excState)
{
    // il processo che la chiama cede il posto nella cpu e va in fondo alla ready_queue
    current_process->p_s = *excState; // salvo lo stato del processo prima di cederlo
    insertProcQ(&ready_queue, current_process);
    scheduler();
};

void syscall_exception_handler(state_t *excState)
{
    int isBlocking = FALSE;
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
        // // Avanzo nel program counter di una word esplicitamente
        // excState->pc_epc += 4;

        switch (excState->reg_a0)
        {
        case CREATEPROCESS:
            NSYS1(excState);
            break;

        case TERMPROCESS:
            NSYS2(excState);
            isBlocking = TRUE;
            break;

        case PASSEREN: // caso speciale, perche puo essere sia bloccante che non.
            // È bloc sse il semVal alla chiamata è 0 o minore
            int *semAdd = (int *)excState->reg_a1;
            isBlocking = *semAdd <= 0;
            NSYS3(excState);
            break;

        case VERHOGEN:
            NSYS4(excState);
            break;

        case DOIO:
            NSYS5(excState);
            isBlocking = TRUE;
            break;

        case GETTIME:
            NSYS6(excState);
            break;

        case CLOCKWAIT:
            NSYS7(excState);
            isBlocking = TRUE;
            break;

        case GETSUPPORTPTR:
            NSYS8(excState);
            break;

        case GETPROCESSID:
            NSYS9(excState);
            break;

        case YIELD:
            NSYS10(excState);
            isBlocking = TRUE;
            break;
        default:
            passUpOrDie(GENERALEXCEPT); // codici SYSCALL non validi
        }

        if (!isBlocking)
        {
            // SOLO PER LE SYSCALL NON BLOCCANTI, per quelle bloccanti il passaggio allo scheduler avviene all'interno della syscall stessa, dopo aver inserito il processo nella ASL
            current_process->p_s = *excState; // aggiorno lo stato del processo corrente(questo vale per le chiamate NON BLOCCANTI che non fanno passare il controllo allo scheduler)
            LDST(&current_process->p_s);      // ricarico lo stato del processo corrente
        }
        // Avanzo nel program counter di una word esplicitamente
        excState->pc_epc += 4;
    }
}

void trap_exception_handler(state_t *excState);

void passUpOrDie(int except_index)
{
    if (current_process->p_supportStruct == NULL)
        NSYS2(&current_process->p_s);
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
    updateCPUTime(); // chiamo updateCPUTime per aggiornare il tempo di CPU per tutti i tipi di eccezione, in questo modo evito di doverlo chiamare in ogni gestore di eccezione specifico.
    // Lo stato di eccezione del processore, al momento dell'eccezione, viene salvato all'indirizzo BIOSDATAPAGE
    state_t *excState = GET_EXCEPTION_STATE_PTR(current_process->p_pid);

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
