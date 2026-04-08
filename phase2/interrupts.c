#include "./headers/interrupts.h";
unsigned int bitmap_value;
int DevNo;
bool IS_DISK(unsigned int id)
{
    return id >= SEM_DISK_0 && id <= SEM_DISK_7;
}

bool IS_FLASH(unsigned int id)
{
    return id >= SEM_FLASH_0 && id <= SEM_FLASH_7;
}

bool IS_ETHERNET(unsigned int id)
{
    return id >= SEM_ETHERNET_0 && id <= SEM_ETHERNET_1;
}

bool IS_PRINTER(unsigned int id)
{
    return id >= SEM_PRINTER_0 && id <= SEM_PRINTER_7;
}

bool IS_TERMINAL_RX(unsigned int id)
{
    return id >= SEM_TERM_RX_0 && id <= SEM_TERM_RX_7;
}

bool IS_TERMINAL_TX(unsigned int id)
{
    return id >= SEM_TERM_TX_0 && id <= SEM_TERM_TX_7;
}

bool IS_TERMINAL(unsigned int id)
{
    return id >= SEM_TERM_RX_0 && id <= SEM_TERM_TX_7;
}

void handleInterrupt()
{
    int IntlineNo;
    // calcolo l'Interrupt Exception Code del device che ha lanciato l'eccezione
    unsigned int excCode = getCAUSE() & CAUSE_EXCCODE_MASK;
    // traduco l'Interrupt Exception Code nell'IntlineNo
    switch (excCode)
    {
    case IL_CPUTIMER:
        IntlineNo = 1;
        break;
    case IL_TIMER:
        IntlineNo = 2;
        break;
    case IL_DISK:
        IntlineNo = 3;
        break;
    case IL_FLASH:
        IntlineNo = 4;
        break;
    case IL_ETHERNET:
        IntlineNo = 5;
        break;
    case IL_PRINTER:
        IntlineNo = 6;
        break;
    case IL_TERMINAL:
        IntlineNo = 7;
        break;

    default:
        break;
    }

    unsigned int word;
    /* Calcolo l'indirizzo fisico in memoria della Interrupting Devices Bit Map relativa
    alla linea di interrupt (IntlineNo) interessata*/
    switch (IntlineNo - 3)
    {
    case 0:
        word = 0x10000040;
        break;
    case 1:
        word = 0x10000040 + 0x04;
        break;
    case 2:
        word = 0x10000040 + 0x08;
        break;
    case 3:
        word = 0x10000040 + 0x0C;
        break;
    case 4:
        word = 0x10000040 + 0x10;
        break;
    default:
        break;
    }

    if (IntlineNo == 1) // 7.2 Processor Local Timer (PLT) Interrupts
    {
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
        current_process->p_s = *((state_t *)GET_EXCEPTION_STATE_PTR(0));
        insertProcQ(&ready_queue, current_process);
        scheduler();
    }
    else if (IntlineNo == 2) // 7.3 System-wide Interval Timer (Pseudo-clock)
    {
        LDIT(PSECOND);

        pcb_t *unblocked_pcb;
        while (unblocked_pcb = removeBlocked(&device_semaphores[SEM_PSEUDOCLOCK]) != NULL)
        {
            insertProcQ(&ready_queue, unblocked_pcb);
            soft_block_counter--;
        }

        /* LDST restituisce il controllo al processo correntemente in esecuzione, che è stato interrotto dal timer,
        aggiornando il suo stato con lo stato di eccezione salvato all'indirizzo BIOSDATAPAGE*/
        if (current_process != NULL)
            LDST((state_t *)GET_EXCEPTION_STATE_PTR(0));
        else
            scheduler();
    }
    else if (IntlineNo >= 3 && IntlineNo <= 7)
    {
        // 7.1 Non-Timer Interrupts
        /* ispeziona la parola estratta per trovare quale singolo bit è acceso a 1, restituendo
    l'indice di quel bit, ovvero il numero del dispositivo (da 0 a 7) che ha generato l'interrupt*/
        bitmap_value = *((unsigned int *)word);
        DevNo = -1;

        if (bitmap_value & DEV0ON)
            DevNo = 0;
        else if (bitmap_value & DEV1ON)
            DevNo = 1;
        else if (bitmap_value & DEV2ON)
            DevNo = 2;
        else if (bitmap_value & DEV3ON)
            DevNo = 3;
        else if (bitmap_value & DEV4ON)
            DevNo = 4;
        else if (bitmap_value & DEV5ON)
            DevNo = 5;
        else if (bitmap_value & DEV6ON)
            DevNo = 6;
        else if (bitmap_value & DEV7ON)
            DevNo = 7;

        // Calcolo the starting address of the device’s device register
        unsigned int devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10);
        unsigned int status;
        int semIndex;

        /*Tutti i dispositivi come dischi, memorie flash, schede di rete e stampanti condividono la stessa
        struttura dei registri (dtpreg_t), composta da STATUS, COMMAND, DATA0 e DATA1.*/
        if (IntlineNo >= 3 && IntlineNo <= 6)
        {
            dtpreg_t *device_reg = (dtpreg_t *)devAddrBase;
            status = device_reg->status;
            device_reg->command = ACK;
            semIndex = (IntlineNo - 3) * DEVPERINT + DevNo;
        }
        else if (IntlineNo == 7)
        {
            termreg_t *term_reg = (termreg_t *)devAddrBase;
            unsigned int tx_status_code = term_reg->transm_status & 0xFF;
            if (tx_status_code != READY && tx_status_code != BUSY)
            {
                status = term_reg->transm_status;
                term_reg->transm_command = ACK;
                semIndex = SEM_TERM_TX_0 + DevNo;
            }
            else
            {
                status = term_reg->recv_status;
                term_reg->recv_command = ACK;
                semIndex = SEM_TERM_RX_0 + DevNo;
            }
        }

        int *semaddr = &device_semaphores[semIndex];
        pcb_t *unblocked_pcb = removeBlocked(semaddr);
        if (unblocked_pcb != NULL)
        {
            unblocked_pcb->p_s.reg_a0 = status;
            insertProcQ(&ready_queue, unblocked_pcb);
            soft_block_counter--;
        }
        if (current_process != NULL)
            LDST((state_t *)GET_EXCEPTION_STATE_PTR(0));
        else
            scheduler();
    }
}
