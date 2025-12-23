#include "./headers/asl.h"
#include "./headers/pcb.h"

 /* Dichiarazione funzioni di debug da klog.c */
     extern void klog_print(char *str);
     extern void klog_print_dec(unsigned int num);
     extern void klog_print_hex(unsigned int num);



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
    // itero sui semafori attivi per cercare un semaforo = semAdd
    struct list_head* iter;
    // int i = 0;
    list_for_each(iter, &semd_h) {
        semd_t* actAdd = container_of(iter, semd_t, s_link);
        if (actAdd->s_key == semAdd) {
            p->p_semAdd = semAdd;
            insertProcQ(&actAdd->s_procq, p);
            return FALSE;
        }
        // klog_print((char*)i);
        // i++;
    }
    // se fallisce non è negli attivi quindi
    // prende un nuovo descrittore di semaforo dalla semdFree
    // e lo inserisce nella semd_h
    if (emptyProcQ(&semdFree_h)) return TRUE;
    // estrai semaforo dalla lista dei liberi, inizializzalo e inseriscilo nella lista dei semafori attivi
    semd_t* newSem = container_of(semdFree_h.next, semd_t, s_link);
    list_del(semdFree_h.next);
    newSem->s_key = semAdd;
    // INIT_LIST_HEAD(&newSem->s_link);
    INIT_LIST_HEAD(&newSem->s_procq);
    list_add(&newSem->s_link, &semd_h);

    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        
        if (container_of(iter, semd_t, s_link)->s_key == semAdd) {
            pcb_t* removed = removeProcQ(&container_of(iter, semd_t, s_link)->s_procq);
            if (emptyProcQ(&container_of(iter, semd_t, s_link)->s_procq)){
                struct list_head freed_sem = container_of(iter, semd_t, s_link)->s_link;
                list_del(&freed_sem);
                // cancello il descrittore del semaforo
                int* key = container_of(&freed_sem, semd_t, s_link)->s_key;
                key = NULL;
                list_add(&freed_sem, &semdFree_h);
                return removed;
            }
        }
    }
    return NULL;
}

pcb_t* outBlocked(pcb_t* p) {
    // struct list_head* iter;
    // list_for_each(iter, &semd_h) {
    //     struct list_head actProcQ = container_of(iter, semd_t, s_link)->s_procq;
    //     if (container_of(iter, semd_t, s_key) == p->p_semAdd) {
    //         return outProcQ(&actProcQ, p);
    //     }
    // }
      return NULL;
}
pcb_t* headBlocked(int* semAdd) {
    struct list_head* iter;
    list_for_each(iter, &semd_h) {
        semd_t* actAdd = container_of(iter, semd_t, s_link);
        if (actAdd->s_key == semAdd) {
            return headProcQ(actAdd->s_procq.next);
        }
    }
    return NULL;
}
