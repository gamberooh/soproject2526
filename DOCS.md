# Documentazione

Gruppo composto da: Davide Gamberini, Riccardo Marchesini, Zeyad Ayad

# Gestione della struttura PCB

## Allocazione e deallocazione PCB

### `freePcb(p)`

Inserisce l’elemento puntato da `p` nella lista `pcbFree_h`.

### `allocPcb()`

Una funzione che controlli se la lista dei liberati è vuota, se lo è restituisce `NULL` , altrimenti prende il primo elemento e ne inizializza tutti i campi e lo restituisce per l’uso. Il campo `p_id`  viene incrementato ogni volta.

### `initPcbs()`

Inizializza la lista dei processi liberi come una lista vuota tramite la macro `INIT_LIST_HEAD`. Successivamente la popola aggiungendo i processi liberi contenuti all’inerno della `pcbFree_table` tramite la macro `list_add()`.

## Gestione code processi

Per la gestione di queste code, abbiamo implementato una logica basata sulla priorità: la funzione `insertProcQ()` assicura che i processi siano ordinati in base al campo `p_prio`, garantendo al contempo una gestione FIFO tra processi con lo stesso livello di priorità. Questo sistema di code permette un’estrazione rapida dalla testa(`removeProcQ()`) o la rimozione di un elemento specifico in qualsiasi posizione della lista (`outProcQ()`).

### `mkEmptyProcQ(head)`

Inizializza la lista la cui sentinella è passata per parametro come una lista vuota tramite la macro `INIT_LIST_HEAD`.

### `emptyProcQ(head)`

Utilizziamo la macro `list_empty()` che restituisce **TRUE** se non ci sono elementi all’interno della lista la cui sentinella è passata per parametro, **FALSE** altrimenti.

### `insertProcQ(head, p)`

L’obiettivo è quello di inserire l’elemento `p` all’interno della lista dei processi attivi, seguendo la politica **fair** delle priorità. Con il puntatore `iter` teniamo traccia della posizione attuale all’interno della lista. Inseriamo in testa all’elemento `iter` se e solo se l’elemento `p` ha priorità maggiore di quello esaminato attualmente, altrimenti continua ad iterare fino a che non arriviamo al termine della lista, dove inseriremo in coda.

### `headProcQ(head)`

Ritorna `NULL` se la lista passata per parametro è vuota, altrimenti ritorna il primo elemento della lista, ovvero il `next` della sentinella.

### `removeProcQ(head)`

Rimuove il primo elemento della lista, altrimenti restituisce `NULL`.

### `outProcQ(head, p)`

Itera sulla lista `head`, comparando il `pid` dell’elemento attuale `iter`e lo rimuove con la macro `__list_del` collegando `iter->next` e `iter->prev`.

## Gestione parentela

I PCB sono anche strutturati  in un sistema di “parentela” chiamato albero dei processi. 

Per realizzare questa struttura, ogni PCB utiizza 3 puntatori specifici:

- `p_parent`: Punta direttamente al processo padre che ha generato il figlio.
- `p_child`: punta al PRIMO dei figli del processo
- `p_sib`: collega tra loro i processi “fratelli”, ovvero quei processi che condividono lo stesso padre.

Questa rete viene gestita attraverso tre funzioni principali:

### `emptyChild(p)`

Usa la macro `list_empty()` per ritornare se la lista puntata da `p` non ha figli.

### `insertChild(parent,p)`

Quando questa funzione viene chiamata, il processo p viene impostato come figlio del parent. `p` viene inserito in testa alla lista dei figli e viene collegato agli altri fratelli già esistenti tramite puntatori `p_sib`.

### `removeChild(p)`

Innanzitutto controlla se `p` ha figli, allora scollega il primo elemento della sua lista dei figli,  annulla il suo campo `parent` e lo restituisce, altrimenti resitutisce `NULL`.

### `outChild(p)`

Permette di rimuovere un processo specifico `p` da qualsiasi punto della lista dei fratelli utilizzando la macro `list_del` che elimina il nodo dalla sua lista di appartenenza.

---

# Gestione dei semafori (ASL)

## La lista dei semafori attivi

### `initASL()`

Inizializza come liste vuote le liste dei semafori attivi e dei semafori liberi, successivamente sposta i semafori dalla lista `s_link` della tabella `semd_table` alla lista `semdFree_h` dei semafori liberi.

### `insertBlocked(semAdd, p)`

Itera sulla lista dei semafori attivi attraverso la macro `list_for_each()`, cercando, tramite il parametro `s_link`, il semaforo passato per parametro. Se viene trovato, aggiunge il processo `p` passato per parametro alla sua lista dei processi attivi. Ritorna `FALSE`.

Se il ciclo finisce vuol dire che il semaforo non è stato trovato. Dunque estrae un nuovo semaforo dalla lista dei semafori liberi e, tramite un suo puntatore appena istanziato, vengono inizializzati i suoi parametri. Il nuovo semaforo viene aggiunto alla lista dei semafori attivi tramite la macro `list_add_tail`.

### `removeBlocked(semAdd)`

Itera sulla lista dei semafori attivi atraverso la macro `list_for_each()` , cercando tramite il parametro `s_link` , il descrittore del semaforo passato per parametro. Se questo corrisponde a quello attualmente esaminato, rimuovo il primo elemento della lista dei processi con la funzione `removeProcQ` precedentemente creata nel modulo PCB e annulla li suo campo `semAdd` . Nel caso in cui in seguito alla rimozione la lista dei processi bloccati sul semaforo attualmente esaminato è vuota, attuo la procedura di rimozione di un semaforo dalla lista dei semafori attivi e lo aggiungo in coda a quella dei liberati. Restituisco il PCB rimosso, nel caso in cui trovo la lista.

**N.B.** Non viene effettuato alcun controllo sulla lista dei processi attivi perché per specifica sappiamo che se un semaforo è attivo, la sua lista dei processi bloccati sarà necessariamente NON vuota.

### `outBlocked(p)`

Itera sulla lista dei semafori per trovare il semaforo, la cui lista dei suoi processi attivi contiene p. Tramite la macro `list_del` il processo viene rimosso da tale lista, senza il bisogno di effettuare due cicli annidati. Se `p` era l’unico elemento di `s_procq`, allora vuol dire che il semaforo è inattivo e quindi viene liberato. Se `p` non è trovato, ritorna **NULL**.

### `headBlocked(semAdd)`

Itera sulla lista dei processi bloccati sul semaforo il cui indirizzo è passato per parametro. Se il processo è trovato, ritorna la testa di tale lista. Altrimenti ritorna **NULL**.

---

# Fase 2

## Inizializzazione del Nucleo

## Scheduler

## Exception handler

## Interrupt handler