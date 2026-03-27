#include "./headers/scheduler.h"
void scheduler()
{

    current_process = removeProcQ(&ready_queue); // prendo il primo processo dalla ready queue e lo metto in esecuzione
    if (current_process != NULL)
    {                                // se c'è un processo da eseguire
        setTIMER(TIMESLICE);         // imposto il timer a TIMESLICE (definito in types.h) per garantire che il processo venga eseguito per un tempo limitato
        LDST(&current_process->p_s); // carico lo stato del processo corrente (current_process) e lo metto in esecuzione, LDST è una funzione che carica lo stato del processo e lo mette in esecuzione, è definita in liburiscv.h
    }
    else
    {
        if (process_counter == 0)
        { // se non ci sono processi da eseguire e non ci sono processi in attesa, significa che il sistema è vuoto e possiamo spegnere la CPU
            HALT();
        }
        if (soft_block_counter > 0 && process_counter > 0)
        {
            // il processo sta aspettando un device interrupt. prima di chiamare WAIT() devo importare il MIE register e disabilitare il PLT
            setMIE(MIE_ALL & ~MIE_MTIE_MASK); // abilito gli interrupt ma disabilito il PLT
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK; // abilito globalmente gli interrupt
            setSTATUS(status);

            WAIT();
        }
        if (soft_block_counter == 0 && process_counter > 0)
        {
            PANIC();
        }
    }
}