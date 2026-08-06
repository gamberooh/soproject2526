#ifndef INITPROC_H_INCLUDED
#define INITPROC_H_INCLUDED

#include "../../headers/types.h"

/* Semafori globali del Support Level */
extern int masterSemaphore;
extern int shellSemaphore;
extern int flashMutex[UPROCMAX];
extern int termReadMutex;
extern int termWriteMutex;

/* Pool di Support Structure (free-list, stesso pattern di fase 1 pcb.c) */
void           initSupportStructs(void);
support_t     *allocSupportStruct(void);
void           freeSupportStruct(support_t *s);

/* Helper condivisi per l'inizializzazione di un U-proc  */
void initUprocState(state_t *s, int asid);
void initUprocSupport(support_t *supp, int asid);

/* Instantiator Process */
void test(void);

#endif
