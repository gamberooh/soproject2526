# Documentazione

Gruppo composto da: Davide Gamberini, Riccardo Marchesini, Zeyad Ayad

# Fase 1: Gestione della struttura PCB

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

# Fase 1: Gestione dei semafori (ASL)

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

# Fase 2: Premesse

Per scrivere il codice di fase 2 abbiamo voluto seguire il design utilizzato nella fase 1, ovvero quello dichiarare le firme delle funzioni che verranno implementate nei file `.c` , in appositi file `.h` .

# Fase 2: Inizializzazione del nucleo

### `memcpy(*dest, *src,len)`

Funzione di utility: viene utilizzata perché non trovata dal compilatore gcc.

- Source: https://github.com/gcc-mirror/gcc/blob/master/libgcc/memcpy.c

### `void populate_puv(puv)`

Prende come input un puntatore al passup vector e setta il campo `tlb_refill_handler`  al Tlb Refill handler vuoto che viene fornito all’interno del test file.

### `void set_sp_tlb_refill(puv)`

Prende come input un puntatore al passup vector e ne inizializza il campo `tlb_refill_stackPtr` all’indirizzo `KERNELSTACK` (0x20001000).

### `void connect_exception_handler(puv)`

Prende come input un puntatore al passup vector e

- Ne inizializza il campo `exception_handler` all’indirizzo di memoria su cui risiede l’exception Handler che abbiamo dovuto creare nel corso di fase 2
- Ne inizializza il campo `exception_stackPtr`  inzializzandolo a `KERNELSTACK` .

### `void init_data_structures(void)`

Richiama le funzioni di inizializzazione di fase 1

- `initPcbs()`
- `initASL()`

### `void init_global_vars()`

Inizializza le variabili globali come da consegna

- `process_counter`
- `soft_block_counter`
- `ready_queue`
- `current_process`
- `device_semaphores`

### `void load_interval_time(devregarea)`

Prende in input un puntatore `devregarea` e lo inzializza a `PSECOND` (100000) sfruttando la macro `LDIT()` .

### `void init_new_proc(void)`

Inizializzo un nuovo processo settandolo in Kernel Mode, mettendo il suo Stack Pointer a ramtop e aumentando il contatore dei processi inizializzati.

### `main()`

All’interno del main vengono richiamate tutte le funzioni dicharate precedentemente oltre allo scheduler che si occuperà della gestione del flusso dei processi.

---

# Fase 2: Scheduler

### `void updateCPUTime()`

 funzione che aggiorna il tempo di CPU accumulato dal processo corrente, misurando il tempo trascorso da quando è stato messo in esecuzione. 

funzionamento: legge il timestamp attuale via `STCK(now)`. accumola nel processo: p_time+=(now-init_time). infine azzera il reference timestamp: init_time=now

viene chimata all’inizio di ogni gestione di eccezione, garantendo che il tempo sia sempre aggiornato prima di prendere decisioni di scheduling.

### `scheduler()`

1. estrae il primo processo dalla coda dei processi pronti. Se vuota, current_process=NULL
2. se un processo è disponibile,  `setTimer(TIMESLICE)` configura il timer di sistema (100ms) per causare un interrupt di clock quando scade. implementa la politica round-robin con time-slice. quando il timer scade→interrupt PLT→il processo va in coda di ready. LDST: carica lo stato completo del processo e trasferisce il controllo dalla CPU al processo.
3. se nessun processo disponibile, allora il sistema è vuoto, tutti i processi terminati →HALT()
4. se ci sono processi (process_counter>0) e sono bloccati in I/O (soft_block_counter>0)→ `setMIE(MIE_ALL & ~MIE_MTIE_MASK`) abilito gli interrupt ma disabilito il PLT. chiamo la WAIT() finchè non arriva un interrupt di device. il proesso si sblocca e va in readyqueue.
5. se ci sono processi MA nessuno bloccato su I/O(soft_block_counter == 0 && process_counte) → deadlock rilevato→nessun processo runnabile e nessuno in attesa di interrupt → chiama PANIC() per terminale il kernel

---

# Fase 2: Exception Handler

### `syscall_exception_handler(mode)`

Viene richiamata dall’`exception_handler` per la gestione delle eccezioni di tipo **SYSCALL.**

Nel caso in cui il resitro `a0` contenga un numero negativo e la modalità è privilegiata, gestiamo ogni system call in maniera indipendente con la relativa funzione. Per le system call non bloccanti, ovvero quelle che in seguito alla chiamata di system call dovranno continuare la propria esecuzione, devo aggiornare lo stato del processo corrente.

Gestisce le chiamate di sistema (Syscall) effettuate dai processi.

- **Logica di Controllo:** Verifica se il processo è in **Kernel Mode** tramite `previousMode`. Se un processo utente prova a chiamare syscall privilegiate (numeri negativi), simula un `PRIVINSTR` trap.
- **Avanzamento PC:** Incrementa `pc_epc += 4` per evitare che, al ritorno, la CPU riesegua all'infinito la stessa syscall.
- **Smistamento:** Usa uno `switch(syscallNum)` per chiamare le funzioni specifiche (`NSYS1NSYS10`).
- **Gestione Bloccante:** Usa il flag `isBlocking`. Se la syscall non è bloccante (es. `GetTime`), aggiorna lo stato del processo e lo riavvia subito con `LDST`. Se è bloccante (es. `Wait`), il controllo passa allo scheduler dentro la funzione specifica.

### `trap_exception_handler(excState)`

Nel caso in cui il contenuto del registro `a0` rientri nei casi descritti nella `__CAUSE_IS_TRAP__` , chiama `passUpOrDie(GENERALEXCEPT)` che deciderà se passare l'errore al Support Level  o terminare definitivamente il processo e la sua progenie.

### `exception_handler()`

Viene preventivamente aggiornato il tempo della CPU del processo in modo da ottenere l’esatto momento in cui il processo viene preso in carico dal gestore delle eccezioni. In secondo luogo viene controllata la causa dell’eccezione utilizzando la macro `CAUSE_IS_INT` presente nativamente in u-RISCV e quelle da noi definite in `exception.h`, passando opportunamente lo stato della CPU al momento della gestione.

## Exception Handler: Funzioni ausiliarie

### `__CAUSE_IS_TLB__(causeCode), __CAUSE_IS_SYSCALL__(causeCode), __CAUSE_IS_TRAP__(causeCode)`

Queste funzioni ausiliarie di controllo prendono in input un codice che se rientra nel range tra 24 e 28 si tratta di un eccezione sollevata da una TLB Refill Exception, se invece si tratta dei codici 8 e 11, abbiamo che l’eccezione è stata sollevata da una SYSCALL, mentre per tutte le altre, ovvero i codici da 0 a 7, 9, 10 e da 12 a 23 sono i codici relativi ad eccezioni lanciate da Trap.

### `int isDeviceSemaphore(int* semAddr)`

Prende in input un descrittore di semaforo e confronta che il valore combaci con uno dei semafori dedicati ai device.

### `pcb_t* getRoot(current)`

Prende in input il pcb da cui è stata lanciata l’eccezione e risale alla radice dell’albero dei processi.

### `pcb_t* findByPid(root, pid)`

Mediante la funzione `getRoot` e il pid su cui vogliamo che la SYSCALL abbia effetto.

Viene effettuata una ricerca in ampiezza sull’albero dei processi per ricercare il pcb con quel determinato pid in tempo lineare.

### `pcb_t* killProgeny(term)`

Mediante la funzione `findByPid` ottengo il pcb da terminare assieme a tutta la sua progenie ovvero figli e figli dei figli tenendo conto anche dei processi che sono rimasti bloccati sui semafori.

Se quindi il processo è rimasto bloccato su di un semaforo ci teniamo salvato il descrittore con un puntatore e lo eliminiamo dalla lista dei processi bloccati su quel semaforo tramite la funzione `outBlocked` definita nella fase 1. 

Nel caso in cui il semaforo sia un semaforo dei device decrementiamo la variabile dei `soft_block_counter` , altrimenti se questo processo è presente nella ready queue viene rimosso.

Infine decrementiamo la variabile che indica il numero dei processi attivi `process_counter` e liberiamo il pcb con la funzione `freePcb` definita in fase 1.

---

## Exception Handler: System Calls

Lo stato della CPU salva il contenuto di 4 registri speciali dedicati alle operazione general purpose. Le system calls sfruttano questi registri per passare informazioni necessarie alla continuazione del ciclo di vita di uno o più processi.

### `void NSYS1(excState)`

**CREATEPROCESS(-1):** 

Come prima cosa viene effettuato un controllo sull’operazione di `allocPcb()` , quindi se non ci sono risorse libere, viene reinserito il valore -1 all’interno del registro `a0` .

In caso di allocazione del pcb ben riuscita, otteniamo stato, priorità e struttura del livello di supporto del nuovo processo dai registri `a1`, `a2` e `a3` , inizializzando ad un valore nullo i restanti campi, per poi incrementare il contatore dei processi attivi, inserire il nuovo processo in ready queue e all’interno lista dei figli del processo che ha lanciato la SYSCALL, ovvero il processo corrente.

### `void NSYS2(excState)`

**TERMINATEPROCESS(-2):**

Nel registro `a1`viene riportato un valore intero corrispondente al pid del processo che deve essere terminato. 

Da specifica se il pid è 0, il processo da terminare è quello corrente, altrimenti viene effettuata una ricerca partendo dalla radice dell’albero utilizzando le funzioni ausiliarie `getRoot` e `findByPid` .

Se il processo viene trovato viene rimosso dall’albero dei processi con la funzione `outChild` definita in fase 1, in modo da isolarlo da altri processi attivi, dopodiché utilizzo la funzione `killProgeny` per terminare ricorsivamente tutti i figli.

Infine viene richiamato lo `scheduler` per la gestione della ready queue.

### `void NSYS3(excState)`

**PASSEREN(-3)**

Nel registro `a1` viene inserito il descrittore del semaforo da cui il processo corrente vorrebbe ottenere una risorsa.

Se la risorsa non è disponibile, ovvero il semaforo ha valore 0, devo bloccare il processo sulla coda dei processi del semaforo. Se il semaforo in questione è uno di quelli destinati ai device, viene incrementato anche il `soft_block_counter` e viene richiamato lo `scheduler` per la gestione della ready queue.

In caso contrario, la risorsa è disponibile, il valore del semaforo viene decrementato e il controllo rimane al processo corrente, che ora è in grando di continuare il suo ciclo di vita dopo aver ottenuto la risorsa.

### `void NSYS4(excState)`

**VERHOGEN(-4)**

Nel registro `a1` viene inserito il descrittore del semaforo a cui il processo corrente deve rilasciare una risorsa.

Se la coda dei processi bloccati su quel determinato semaforo non è vuota, libero il primo processo bloccato con `removeBlocked` e lo inserisco nella ready queue. Se questo semaforo corrisponde ad un semaforo per i device, devo decrementare il `soft_block_counter` . In caso contrario incremento il valore del semaforo.

### `void NSYS5(excState)`

**DOIO(-5)**

Gestisce l'input/output. Calcola l'indice del dispositivo tramite l'indirizzo di memoria (`commandAddr`), identifica se si tratta di un terminale (gestendo separatamente Ricezione e Trasmissione) e calcola l'indice del semaforo corrispondente in `device_semaphores`. Esegue una `P` (`NSYS3`) sul semaforo del device per bloccare il processo fino al completamento dell'operazione hardware.

### `void NSYS6(excState)`

**GETCPUTIME(-6)**

Restituisce nel registro `a0` il tempo totale di CPU accumulato dal processo (`p_time`), aggiornato automaticamente ad ogni ingresso in exception handler.

### `void NSYS7(excState)`

**PSEUDOCLOCK(-7)**

Blocca il processo sul semaforo dello **Pseudo-Clock** (`SEM_PSEUDOCLOCK`). Incrementa `soft_block_counter` e chiama lo `scheduler`. Il processo verrà sbloccato ogni 100ms dall'interrupt del timer.

### `void NSYS8(excState)`

**GETSUPPORTDATA(-8)**

Restituisce l'indirizzo della `supportStruct` del processo corrente, permettendo l'interazione con i livelli superiori (Fase 3).

### `void NSYS9(excState)`

**GETPROCESSID(-9)**

Se il parametro in `a1` è 0, restituisce il PID del processo attuale; altrimenti restituisce il PID del padre (o 0 se il processo è root).

### `void NSYS10(excState)`

**YIELD(-10)**

Implementa il rilascio volontario della CPU. Salva lo stato attuale, inserisce il processo in fondo alla `ready_queue`  sfruttando il fatto che gli assegnamo la priorità minima. Viene chiamato lo `scheduler` per servire il prossimo processo.

---

# Fase 2: Interrupt Exception Handling e Pass Up Or Die

### **`handleInterrupt()`**

Questo metodo gestisce gli interrupt hardware, ovvero I/O, PLT e Interval Timer. Inizialmente calcola l'Interrupt Exception Code, lo traduce in **`IntlineNo`** e individua la *Interrupting Devices Bit Map* corrispondente. Successivamente, si dirama in base al tipo di interrupt:

- **PLT (Processor Local Timer):** Riconosce l'interrupt ricaricando il timer a **`TIMESLICE`**. Copia lo stato del processore nel PCB corrente, lo declassa nella *Ready Queue,* da *running* a *ready,* e chiama lo Scheduler.
- **Interval Timer (Pseudo-clock):** Riconosce l'interrupt caricando **`PSECOND`** tramite la macro **`LDIT`**. Sblocca tutti i processi in attesa del *tick* dello Pseudo-clock, dopodiché restituisce il controllo al processo corrente tramite **`LDST`** se il sistema non è in stato di **`WAIT`**.
- **Dispositivi Periferici (I/O):**
    1. Calcola l'indirizzo del registro del dispositivo, ne salva lo stato e riconosce l'interrupt scrivendovi il comando di **`ACK`**.
    2. Esegue un'operazione *V* sul semaforo del dispositivo per sbloccare il processo in attesa, inserendolo nella ready queue, senza usare la SYSCALL, in modo da avere restituito il puntatore al processo `unblocked_pcb`.
    3. Posiziona il codice di stato salvato nel registro **`a0`** del processo appena sbloccato e lo inserisce nella *Ready Queue*.
    4. Restituisce il controllo al processo corrente tramite **`LDST`**, oppure chiama lo Scheduler se non c'è alcun processo in esecuzione.

### **`passUpOrDie(except_index)`** 
Per le eccezioni SYSCALL ≥ 1, Program Trap e TLB, il comportamento del Nucleo dipende dalla presenza della Support Structure all’interno del pass up vector del processo. Se assente, il processo corrente e tutta la sua progenie vengono terminati. Se presente, il Nucleo salva lo stato dell'eccezione nella struttura ed esegue un'istruzione **`LDCXT`** per passare il controllo alla routine di gestione definita dal Livello di Supporto.

---

# Fase 3: Premesse
Per scrivere il codice del livello di supporto abbiamo seguito lo stile di progettazione utilizzato anche nelle fasi precedenti cercando di mantenere una struttura del codice pulita dividendo le intestazioni `.h` dai file di codice `.c`.

## Inizializzazione dei processi utente
### Variabili globali del livello di supporto
- `masterSemaphore` semaforo che gestisce l'accesso mutualmente esclusivo all'esecuzione del processo master (test) deve attendere in coda su di esso finché non termina la shell e libera la risorsa con la primitiva `V()`.
- `shellSemaphore` semaforo che gestisce l'accesso mutualmente esclusivo ai programmi in esecuzione sulla shell. Quando un programma termina, rilascia la risorsa che torna alla shell che quindi riprende la sua esecuzione.
- `flashMutex[UPROCMAX]` semaforo che gestisce lettura e scrittura su di un flash device per evitare che due processi scrivano sullo stesso device contemporaneamente. Per scrivere sul device `i` il processo deve prima invocare la primitva `P()` sul semaforo corrispondente.
- `termReadMutex` semaforo che gestisce l'accesso in lettura sul terminale.
- `termWriteMutex` semaforo che gestisce l'accesso in scrittura sul terminale.
- `suppStruct_table[UPROCMAX]` sttura di supporto per ogni processo utente, utile per la gestione delle eccezioni di tipo `pagefault` o `generalexcept` e per la gestione della tabella delle pagine private.
- `suppStructFree_h` lista di tutte le pagine di supporto libere, che verranno allocate e liberate dinamicamente come suggerito nelle specifiche di progetto.
- `supportStructSem` semaforo per la gestione delle support struct libere: evita l'allocazione contemporanea da parte di due processi della stessa support struct.

### **`initSupportStructs()`**
Prende tutte le support struct definite all'interno della tabella, le inserisce all'interno della lista delle strutture di supporto libere e inizializza ad `1` il semaforo `supportStructSem` in modo tale da assicurare la possbilità ad un u-proc di allocare una struttura di supporto.

### **`*allocSupportStruct()`**
Richiede la possibilità di ottenere la struttura di supporto invocando la primitiva `P()` sul semaforo.
Una volta ottenuta la risorsa, controlla che ci sia almeno un elemento all'interno della lista delle strutture libere, se non c'è restutisce `NULL`, altrimenti ottiene la struttura e la rimuove dalla lista delle libere evitando conflitti futuri.
Una volta otttenuta la struttura di supporto rilascia il semaforo.

### **`*freeSupportStruct(support_t *s)`**
Mi servo del semaforo per evitare conflitti, ottenuta la risorsa, libero la struttura di supporto, inserendo con la primitiva `list_add_tail` all'interno della lista delle libere. Infine libero l'accesso alla risorsa condivisa.

### **`*initDeviceMutex()`**
Inizializzo i semafori per l'accesso ai flash device dando la possbilità di accesso. Inizializzati allo stesso modo anche i semafori di lettura e scrittura sul terminale (`val = 1`).

### **`initUprocState(state_t *s, int asid)`**
Prende in input lo stato di un processo utente da inizializzare e il suo ASID e inizializza i campi dello stato del processo in modo tale che:
- Pulisca il registro `cause` e i registri generali
- Imposti il `pc` all'indirizzo di partenza del processo utente impostandolo a `UPROCSTARTADDR`.
- Imposti lo stack pointer a `USERSTACKTOP`.
- Abiliti gli interrutpt globali e rimanendo in user mode (MPP = 0).
- Abiti tutti gli interrupt
- Imposti il registro `entry_hi` in modo tale che tramite l'apposito shift, l' `ASID` in input finisca nell'area corretta.

### **`initUprocSupport(support_t *supp, int asid)`**
Prende in input la struttura di supporto ottenuta dal processo e la inizializza:
- Imposta l'asid in input del processo utente come campo della struttura di supporto.

- Configura i campi per la gestione di eccezioni di tipo `pagefault` e di tipo `generalexept`.  Ogni `context_t` contiene i registri stackPtr, status e pc che vengono caricati nel processore quando si verifica l'eccezione corrispondente.

- Fa distinizione all'interno della tabella delle pagine di un proceddo tra text & data area e l'area dedicata allo stack. Per ogni pagina della tabella privata riservata a text & data del processo utente, imposto il campo `entryHI` e `entryLO` della PTE. 
- Per **text & data**`entryHI` contiene l'indirizzo virtuale della pagina (`KUSEG + i*PAGESIZE`) e l'ASID del processo, mentre `entryLO` contiene il flag `DIRTYON` per indicare che la pagina e' scrivibile.
- Per **stack** il VPN è settato staticamente a `USERSTACKTOP - PAGESIZE`

### **`test()`**
Il test è definito come master process e ha il compito di inizializzare le strutture di supporto, i sermafori dei device e le strutture che favoriscono il memory swap impsotando tutti i frame della swap pool come liberi e tutti i frame della RAM come liberi.

[CONTINUA DA SEMAFORI...]