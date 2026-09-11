#include "./headers/exceptions.h"
#include <uriscv/types.h>

/* Metodi di controllo privati */

int __CAUSE_IS_TLB__(unsigned int causeCode)
{
    return (causeCode >= EXC_MOD && causeCode <= EXC_UTLBS);
}

int __CAUSE_IS_SYSCALL__(unsigned int causeCode)
{
    return (causeCode == EXC_ECU || causeCode == EXC_ECM);
}

int __CAUSE_IS_TRAP__(unsigned int causeCode)
{
  return (
    (causeCode >= EXC_IAM && causeCode <= EXC_SAF) ||
    causeCode == EXC_ECS ||                 
    causeCode == PRIVINSTR ||                     
    (causeCode >= EXC_IPF && causeCode < EXC_MOD)
    );
}

/* Funzioni ausiliarie */

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
    // Visita preordine a partire dalla radice dell'albero dei processi
    if (root == NULL)
        return NULL;
    else if (root->p_pid == pid)
        return root;
    else
    {
        struct list_head *iter;
        // Itero su tutti i figli del pcb passato in input
        list_for_each(iter, &root->p_child)
        {
            pcb_t *child = container_of(iter, pcb_t, p_sib);
            pcb_t *found = findByPid(child, pid);

            // Se trovato nel sotto-albero, lo propaghiamo verso l'alto
            if (found != NULL)
                return found; 
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
        // Tolgo il processo da terminare dalla lista
        // dei bloccati del semaforo su cui era bloccato per richiedere una risorsa
        int *semAddr = term->p_semAdd;
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
    pcb_t *newPcb = allocPcb();

    if (newPcb == NULL)
        excState->reg_a0 = CREATEPROCESS;
    else
    {
        newPcb->p_s = *newState;
        newPcb->p_prio = prio;
        newPcb->p_supportStruct = supportLevel;
        // Annullo manualmente i campi che devono essere nulli
        //newPcb->p_time = 0; da annullare, lo fa gia allocPcb
        //newPcb->p_semAdd = NULL; da annullare , lo fa gia allocPcb
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
        term = current_process;
    else
        // Cerco il pcb partendo dalla radice (init)
        term = findByPid(getRoot(current_process), pid);
    
    // Terminazione processo
    if (term != NULL)
    {
        // stacco il processo term dall'albero dei processi creando
        // un'isola di processi che non ha dipendenze da quelli attivi.
        outChild(term);
        killProgeny(term);
    }
    scheduler();
}

// Passeren
void NSYS3(state_t *excState)
{
    int *semAdd = (int*)excState->reg_a1;

    if (*semAdd == 0)
    {
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
     
    if (headBlocked(semAdd) != NULL) 
    {
        insertProcQ(&ready_queue, removeBlocked(semAdd));
        
        if (isDeviceSemaphore(semAdd)) 
        {
            soft_block_counter--;
        }
    }
    else 
    {
        (*semAdd)++;
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
    int commandValue = (int)excState->reg_a2;
    
    *((int*)commandAddr) = commandValue;

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
    // Passo l'indirizzo di memoria dove è salvato il val del semaforo
    excState->reg_a1 = (memaddr) &device_semaphores[semIndex];
    // assegno la risorsa o blocco il processo su quel semaforo
    NSYS3(excState);
}

// GetCPUTime
void NSYS6(state_t *excState)
{
    // non aggiorno qui il tempo perché lo faccio ogni volta
    // all'interno del exception handler
    excState->reg_a0 = (unsigned int)current_process->p_time;
}

// WaitForClock
void NSYS7(state_t *excState)
{
    int *semAdd = &device_semaphores[SEM_PSEUDOCLOCK];
    current_process->p_s = *excState;
    // Se fallisce l'inserimento il controllo passa a passupordie
    if (insertBlocked(semAdd, current_process))
    {
        passUpOrDie(GENERALEXCEPT);
        return;
    }
    soft_block_counter++;
    scheduler();
};

// GetSupportData
void NSYS8(state_t *excState)
{
    excState->reg_a0 = (memaddr)current_process->p_supportStruct;
};

// GetProcessID
void NSYS9(state_t *excState)
{
    int parent = (int)excState->reg_a1;
    if (parent == 0)
        excState->reg_a0 = current_process->p_pid;
    else
    {
        if (current_process->p_parent == NULL) // root
            excState->reg_a0 = 0;
        else
            excState->reg_a0 = current_process->p_parent->p_pid;
    }
};

// Yield
void NSYS10(state_t *excState)
{
    // il processo che la chiama cede il posto nella cpu e va in fondo alla ready_queue
    current_process->p_s = *excState;
    // lo mettiamo a priorità min per avere la sicurezza di metterlo in coda
    current_process->p_prio = PROCESS_PRIO_LOW;
    insertProcQ(&ready_queue, current_process);
    scheduler();
};

void syscall_exception_handler(state_t *excState)
{
    int isBlocking = FALSE;
    int syscallNum = (int)excState->reg_a0;
    unsigned int previousMode = (excState->status & MSTATUS_MPP_MASK);

    // Simula un Program Trap per istruzione privilegiata
    if (syscallNum < 0 && previousMode != MSTATUS_MPP_M)
    {
        excState->cause = PRIVINSTR;
        trap_exception_handler(excState); // Passa il controllo al gestore dei Trap
        return;
    }
    // Controllo istruzione in kernel mode
    if (syscallNum < 0 && previousMode == MSTATUS_MPP_M)
    {
        // Avanzo nel program counter di una word esplicitamente
        excState->pc_epc += 4;

        switch (syscallNum)
        {
        case CREATEPROCESS:
            NSYS1(excState);
            break;

        case TERMPROCESS:
            NSYS2(excState);
            isBlocking = TRUE;
            break;

        case PASSEREN: // caso speciale, perche puo essere sia bloccante che non.
            int *semAdd = (int *)excState->reg_a1;
            isBlocking = *semAdd == 0;
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
            // Aggiorno lo stato del processo corrente
            current_process->p_s = *excState;
            LDST(&current_process->p_s);
        }

        // nel caso bloccante, al termine della funzione viene sempre lanciato lo scheduler
        // che gestirà il continuo del ciclo di vita del processo.
    }
     if (syscallNum >= 0)
    {
        // Codici SYSCALL positivi (SYS1..SYSn degli U-proc, Fase 3). richiesta dal prof.
        passUpOrDie(GENERALEXCEPT);
    }
}

void trap_exception_handler(state_t *excState)
{
    (void)excState;
    passUpOrDie(GENERALEXCEPT);
}

void passUpOrDie(int except_index)
{
    if (current_process->p_supportStruct == NULL)
    {
        state_t termState;
        termState.reg_a1 = 0; /* PID 0 -> corrente */
        NSYS2(&termState);
    }
    else
    {
        state_t *bios_state = (state_t *)GET_EXCEPTION_STATE_PTR(0);
        current_process->p_supportStruct->sup_exceptState[except_index] = *bios_state;

        unsigned int sp = current_process->p_supportStruct->sup_exceptContext[except_index].stackPtr;
        unsigned int status = current_process->p_supportStruct->sup_exceptContext[except_index].status;
        unsigned int pc = current_process->p_supportStruct->sup_exceptContext[except_index].pc;
        LDCXT(sp, status, pc); // context switch
    }
}

void exception_handler()
{
    updateCPUTime(); 
    
    state_t *excState = GET_EXCEPTION_STATE_PTR(0);
    unsigned int excCause = excState->cause;

    if (CAUSE_IS_INT(excCause))
    {
        handleInterrupt();
        return;
    }

    /* Calcolo Exception Code (bit 2-6) */
    unsigned int excCode = (getCAUSE() & CAUSE_EXCCODE_MASK);


    if (__CAUSE_IS_TLB__(excCode))
    {
        passUpOrDie(PGFAULTEXCEPT);
        return;
    }

    if (__CAUSE_IS_SYSCALL__(excCode))
    {
        syscall_exception_handler(excState);
        return;
    }

    if (__CAUSE_IS_TRAP__(excCode))
    {
        passUpOrDie(GENERALEXCEPT);
        return;
    }
}

/* Vero TLB-Refill event handler (Fase 3, sez. 3 delle specifiche). */
void uTLB_RefillHandler(void)
{
    state_t *savedState = (state_t *)GET_EXCEPTION_STATE_PTR(0);// salvo lo stato del processo corrente al momento dell'eccezione TLB-Refill. quello (0) è l'indice della tabella delle eccezioni del BIOS, che contiene lo stato del processo al momento dell'eccezione. 
    /* NB: niente maschera GETPAGENO prima dello shift, taglierebbe via il bit 31
     * (sempre acceso per gli indirizzi kuseg) azzerando il VPN.  */
    unsigned int vpn = savedState->entry_hi >> VPNSHIFT; //entry_hi contiene l'indirizzo virtuale del pezzo che serviva .ricava l'indirizzo virtuale della pagina che ha causato l'eccezione TLB-Refill, spostando a destra di VPNSHIFT bit  il valore del registro entry_hi salvato nello stato del processo. Questo permette di ottenere il numero di pagina virtuale (VPN) corrispondente all'indirizzo che ha causato l'eccezione.
    int idx = (vpn == 0xBFFFF) ? (USERPGTBLSIZE - 1) : (int)(vpn - (KUSEG >> VPNSHIFT)); //se vpn è uguale a 0xBFFFF (che rappresenta l'ultimo indirizzo della zona utente), allora idx viene impostato a USERPGTBLSIZE - 1, altrimenti idx viene calcolato come vpn meno KUSEG >> VPNSHIFT. Questo calcolo determina l'indice della tabella delle pagine utente corrispondente alla pagina virtuale che ha causato l'eccezione TLB-Refill.
    pteEntry_t *pte = &current_process->p_supportStruct->sup_privatePgTbl[idx]; // va a auardare, nella tabella della pagina del processo corrente, cosa ce scritto per quella pagina specifica- presente o assente, e se presente, quale sia l'indirizzo fisico corrispondente.

    setENTRYHI(pte->pte_entryHI);//copia il valore del campo pte_entryHI della voce della tabella delle pagine corrispondente alla pagina virtuale che ha causato l'eccezione TLB-Refill nel registro ENTRYHI, che viene utilizzato per la gestione della TLB.
    setENTRYLO(pte->pte_entryLO);//copia il valore del campo pte_entryLO della voce della tabella delle pagine corrispondente alla pagina virtuale che ha causato l'eccezione TLB-Refill nel registro ENTRYLO, che viene utilizzato per la gestione della TLB.
    TLBWR(); // scrive la voce della tabella delle pagine appena caricata nei registri ENTRYHI e ENTRYLO nella TLB. 

    LDST(savedState);
}
