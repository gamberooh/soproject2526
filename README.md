# SO Project AA 2025/2026
Progetto finale del corso di Sitemi Operativi del secondo anno del corso di Informatica di UniBo che punta ad implementare in tre diverse fasi un sistema operativo virtualizzato tale PandOssh.

## Authors
 - Davide Gamberini
 - Riccardo Marchesini
 - Zeyad Ayad
---
# Fase 1 

 Per la fase 1 abbiamo sviluppato assieme le funzioni, senza dividerci i compiti, perché essendo i moduli sviluppati una base per comprendere al meglio
tutto il resto del progetto, abbiamo preferito aver chiaro la logica di implementazione della coda dei processi, dell'albero dei processi e della gestione dei semafori
e della lista dei processi bloccati, per essere pronti a implementare le prossime fasi.

# Fase 2
Per la fase 2 ci siamo divisi i compiti in questa maniera
- Davide Gamberini: initial, syscall
- Zeyad Ayad: scheduler, syscall
- Riccardo Marchesini: interrupt, pass up or die. \
Per la revisione e debug abbiamo lavorato tutti assieme, concordando sulla modularizzazione del codice similarmente a quello fatto per fase 1. Abbiamo deciso di spezzettare le funzioni di controllo utilizzando delle funzioni di supporto.

# Fase 3
Per la fase 3 abbiamo sviluppato assieme le funzioni di supporto alla virtualizazzione e alle syscall livello utente, shell e programmi di test.

>**NB:** Abbiamo aggiornato il file della documentazione dove abbiamo giustificato ogni nostra scelta implementativa e aggiunto alcuni commenti significativi all'interno del codice.


