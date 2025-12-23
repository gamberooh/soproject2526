#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
  INIT_LIST_HEAD(&pcbFree_h);
  for (int i = 0; i < MAXPROC; i++) {
    list_add(&pcbFree_table[i].p_list, &pcbFree_h);
  }
}

void freePcb(pcb_t* p) {
  list_add_tail(&p->p_list, &pcbFree_h);
}

pcb_t* allocPcb() {
  if (list_empty(&pcbFree_h)) return NULL;
  struct list_head* first = pcbFree_h.next; //primo elem = next di sentinella
  pcb_t* elem = container_of(first, pcb_t, p_list);
  elem->p_parent = NULL;
  INIT_LIST_HEAD(&elem->p_child);
  INIT_LIST_HEAD(&elem->p_sib);
  elem->p_pid = next_pid++;
  elem->p_prio = 0;
  elem->p_supportStruct = NULL;
  //stato cpu
  elem->p_s.cause = 0;
  elem->p_s.entry_hi = 0;
  elem->p_s.mie = 0;
  elem->p_s.pc_epc = 0;
  elem->p_s.status = 0;
  for (int i = 0; i < STATE_GPR_LEN; i++) {
    elem->p_s.gpr[i] = 0;
  }
  elem->p_time = 0;
  elem->p_semAdd = NULL;
  list_del(first);
  return elem;
}

void mkEmptyProcQ(struct list_head* head) {
  INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head* head) {
  return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
  struct list_head* iter;
  list_for_each(iter, head){
    int act_prio = container_of(iter, pcb_t, p_list)->p_prio;
    if (p->p_prio > act_prio) {
      __list_add(&p->p_list, iter->prev, iter);
      return;
    }
  }
  //inserire in coda se ha la prio piu bassa
  list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(struct list_head* head) {
  if (emptyProcQ(head)) return NULL;
  pcb_t* tmp= container_of(head->next, pcb_t,p_list);
  return tmp;
}

pcb_t* removeProcQ(struct list_head* head) {
  if (emptyProcQ(head)) return NULL;
  struct list_head* first = head->next;
  pcb_t* elem = container_of(first, pcb_t, p_list);
  list_del(first);
  return elem;
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
  struct list_head* iter;
  list_for_each (iter, head) {
    if (p->p_pid == container_of(iter, pcb_t, p_list)->p_pid) { //match
      __list_del(iter->prev, iter->next);
      return p;
    }
  }
  return NULL; //no match
}

int emptyChild(pcb_t* p) {
  return list_empty(&p->p_child);
}

void insertChild(pcb_t* prnt, pcb_t* p) {
  //collego alla lista fratelli nuova la lista fratelli del proc p
  //cosicche i fratelli non debbano rimanere orfani
  list_add_tail(&p->p_sib, &prnt->p_child);
  p->p_parent = prnt;
}

pcb_t* removeChild(pcb_t* p) {
 if (emptyChild(p)) return NULL;
  //primo elem = next di sentinella (ovvero p->child)
  struct list_head* firstChild = p->p_child.next;
  // rimuovo firstChild dalla testa -> la struttura si ricuce da sola
  list_del(firstChild);
  // preparp
  pcb_t* elem = container_of(firstChild, pcb_t, p_sib);
  elem->p_parent = NULL;
  return elem;
}

pcb_t* outChild(pcb_t* p) {
  if (p->p_parent == NULL) return NULL;
  list_del(&p->p_sib);
  return p;
}
