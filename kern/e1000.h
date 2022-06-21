#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <kern/pci.h>

int attach_e1000(struct pci_func *pcif);

int transmit(char *buff, int size);

int receive(char *buff, int size);

void e1000_interrupt();

uint16_t read_eeprom_from(int addr);

void read_mac_addr(uint16_t *w0, uint16_t *w1, uint16_t *w2);

physaddr_t uva2pa(struct Env *env, void *va);

#endif	// JOS_KERN_E1000_H
