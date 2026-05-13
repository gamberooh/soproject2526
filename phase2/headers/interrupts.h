#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>

/* Indici di accesso ai semafori*/

/* --- Pseudo-clock --- */
#define SEM_PSEUDOCLOCK 0

/* --- Dischi (Interrupt Line 3) --- */
#define SEM_DISK_0 1
#define SEM_DISK_1 2
#define SEM_DISK_2 3
#define SEM_DISK_3 4
#define SEM_DISK_4 5
#define SEM_DISK_5 6
#define SEM_DISK_6 7
#define SEM_DISK_7 8

/* --- Flash Devices (Interrupt Line 4) --- */
#define SEM_FLASH_0 9
#define SEM_FLASH_1 10
#define SEM_FLASH_2 11
#define SEM_FLASH_3 12
#define SEM_FLASH_4 13
#define SEM_FLASH_5 14
#define SEM_FLASH_6 15
#define SEM_FLASH_7 16

/* --- Network / Ethernet (Interrupt Line 5) --- */
#define SEM_ETHERNET_0 17
#define SEM_ETHERNET_1 18
#define SEM_ETHERNET_2 19
#define SEM_ETHERNET_3 20
#define SEM_ETHERNET_4 21
#define SEM_ETHERNET_5 22
#define SEM_ETHERNET_6 23
#define SEM_ETHERNET_7 24

/* --- Stampanti (Interrupt Line 6) --- */
#define SEM_PRINTER_0 25
#define SEM_PRINTER_1 26
#define SEM_PRINTER_2 27
#define SEM_PRINTER_3 28
#define SEM_PRINTER_4 29
#define SEM_PRINTER_5 30
#define SEM_PRINTER_6 31
#define SEM_PRINTER_7 32

/* --- Terminali - RICEVITORI (Interrupt Line 7) --- */
#define SEM_TERM_RX_0 33
#define SEM_TERM_RX_1 34
#define SEM_TERM_RX_2 35
#define SEM_TERM_RX_3 36
#define SEM_TERM_RX_4 37
#define SEM_TERM_RX_5 38
#define SEM_TERM_RX_6 39
#define SEM_TERM_RX_7 40

/* --- Terminali - TRASMETTITORI (Interrupt Line 7) --- */
#define SEM_TERM_TX_0 41
#define SEM_TERM_TX_1 42
#define SEM_TERM_TX_2 43
#define SEM_TERM_TX_3 44
#define SEM_TERM_TX_4 45
#define SEM_TERM_TX_5 46
#define SEM_TERM_TX_6 47
#define SEM_TERM_TX_7 48


/* --- Limiti per terminal devices --- */

#define SEM_TERM_START 33
#define SEM_TERM_END 48

void handleInterrupt();

#endif