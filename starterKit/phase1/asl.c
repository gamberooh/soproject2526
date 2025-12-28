#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;//sentinella dei semafori liberi
static struct list_head semd_h; //sentinella dei semafori attivi


void initASL() {
    mkEmptyProcQ(&semdFree_h);
    mkEmptyProcQ(&semd_h);
    for (int i = 0; i < MAXPROC; i++) {
        list_add(&semd_table[i].s_link, &semdFree_h);
    }
}

int insertBlocked(int* semAdd, pcb_t* p) {
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        semd_t* actAdd = container_of(iter, semd_t, s_link);
        if (actAdd->s_key == semAdd) {
            p->p_semAdd = semAdd;
            //semafori FIFO
            list_add_tail(&p->p_list, &actAdd->s_procq);
            return FALSE;
        }
    }
    // fallisce = non attivo -> nuovo descrittore di semaforo
    // da semdFree_h e lo inserisce nella semd_h
    if (emptyProcQ(&semdFree_h)) return TRUE;
    // estrai 1' semaforo libero
    semd_t* newSem = container_of(semdFree_h.next, semd_t, s_link);
    list_del(semdFree_h.next);
    newSem->s_key = semAdd;
    mkEmptyProcQ(&newSem->s_procq);
    p->p_semAdd = semAdd; //assegno il nuovo semAdd
    list_add_tail(&p->p_list, &newSem->s_procq); //aggiungo sentinella p ai bloccati di newSem
    list_add_tail(&newSem->s_link, &semd_h); // newSem aggiunto alla ASL
    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        if (container_of(iter, semd_t, s_link)->s_key == semAdd) {
            pcb_t* removed = removeProcQ(&container_of(iter, semd_t, s_link)->s_procq);
            if (emptyProcQ(&container_of(iter, semd_t, s_link)->s_procq)){
                container_of(iter, semd_t, s_link)->s_key = NULL; // elimino il suo semAdd
                list_del(iter);
                list_add_tail(iter, &semdFree_h);
            }
            return removed;
        }
    }
    return NULL;
}

pcb_t* outBlocked(pcb_t* p) {
    struct list_head* iter; 
    list_for_each(iter, &semd_h) {
        if (container_of(iter, semd_t, s_link)->s_key == p->p_semAdd) {
            pcb_t* removed = outProcQ(&container_of(iter, semd_t, s_link)->s_procq, p);
            // caso in cui p era unico nella coda dei bloccati
            if (emptyProcQ(&container_of(iter, semd_t, s_link)->s_procq)){
                container_of(iter, semd_t, s_link)->s_key = NULL;
                list_del(iter);
                list_add_tail(iter, &semdFree_h);
            }
            return removed;
        }
    }
    return NULL;
}
                                                                                                                            
pcb_t* headBlocked(int* semAdd) {
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        if (container_of(iter, semd_t, s_link)->s_key == semAdd) {
            return headProcQ(&container_of(iter, semd_t, s_link)->s_procq);
        }
    }
    return NULL;
}
