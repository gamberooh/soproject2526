#ifndef KLOG_H
#define KLOG_H

/**
 * @brief Prototipi delle funzioni per il circular log buffer.
 */

void klog_print(char *str);
void klog_print_dec(unsigned int num);
void klog_print_hex(unsigned int num);

#endif /* KLOG_H */
