#include "./headers/initial.h"

cpu_t init_time = 0;

void klog_print(char* str);


void updateCPUTime()
{
    cpu_t now;

    if (current_process == NULL)
        return;

    STCK(now);
    current_process->p_time += (now - init_time);
    init_time = now;
}

void scheduler()
{
    current_process = removeProcQ(&ready_queue);
    if (current_process != NULL)
    {
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
        LDST(&current_process->p_s);
    }
    else
    {
        if (process_counter == 0)
        {
            HALT();
        }
        if (soft_block_counter > 0 && process_counter > 0)
        {
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            WAIT();
        }
        if (soft_block_counter == 0 && process_counter > 0)
        {
            PANIC();
        }
    }
}