#ifndef VMSUPPORT_H_INCLUDED
#define VMSUPPORT_H_INCLUDED

#include "../../headers/types.h"

/* Codice della TLB-Modification exception: non esiste una costante fornita
per questo valore, lo faccio io(confermato dal prof su telegram). */
#define EXC_TLBMOD 24


void initSwapStructs(void);


void pager(void);
void freeUprocFrames(int asid);
void tlbUpdate(unsigned int entryHI, unsigned int entryLO);
#endif
