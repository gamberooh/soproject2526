#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
}

void freePcb(pcb_t* p) {
  list_add_tail(&p->p_list, &pcbFree_h);
}

pcb_t* allocPcb() {
}

void mkEmptyProcQ(struct list_head* head) {
  /*if (head != NULL)
    INIT_LIST_HEAD(head);
  //else
    struct list_head lista = LIST_HEAD_INIT(head);*/
}

int emptyProcQ(struct list_head* head) {
  return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
  struct list_head* iter;
  list_for_each(iter, &head){
    int prio = container_of(head, pcb_t, p_list)->p_prio;
    if (prio <= p->p_prio){
      __list_add(p, iter, iter->next);
      break;
      // iter->next=head;
      // iter->next=NULL;
    }
  }
}

pcb_t* headProcQ(struct list_head* head) {
}

pcb_t* removeProcQ(struct list_head* head) {
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
}

int emptyChild(pcb_t* p) {
}

void insertChild(pcb_t* prnt, pcb_t* p) {
}

pcb_t* removeChild(pcb_t* p) {
}

pcb_t* outChild(pcb_t* p) {
}
