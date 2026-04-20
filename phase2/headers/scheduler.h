#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../../headers/types.h"

extern cpu_t init_time;
void updateCPUTime(void);

void scheduler(void);

#endif