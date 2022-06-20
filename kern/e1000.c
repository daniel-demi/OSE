#include <kern/e1000.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <kern/e1000_hw.h>
#include <inc/string.h>
#include <inc/error.h>
#include <kern/env.h>
#include <kern/picirq.h>

#define BAR0_AT(off) (bar0[off / 4])
#define TRANS_QUEUE_SZ 64
#define REC_QUEUE_SZ 128
#define BUFF_SIZE 2048

// LAB 6: Your driver code here
static volatile uint32_t *bar0;
struct e1000_tx_desc *tx_queue_desc;
char tx_buff_queue[TRANS_QUEUE_SZ][BUFF_SIZE];

struct e1000_rx_desc *rx_queue_desc;
char rx_buff_queue[REC_QUEUE_SZ][BUFF_SIZE];

int attach_e1000(struct pci_func *pcif) {
    cprintf("Attaching E1000\n");
	// ex3
	pci_func_enable(pcif);
	
	// ex4
	bar0 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	
	// ex5
	struct PageInfo *tx_queue_page = page_alloc(ALLOC_ZERO);
	BAR0_AT(E1000_TDBAH) = 0;
	BAR0_AT(E1000_TDBAL) = page2pa(tx_queue_page);
	BAR0_AT(E1000_TDLEN) = 64 * sizeof(struct e1000_tx_desc);
	tx_queue_desc = page2kva(tx_queue_page);

	int i;
	for (i = 0; i < TRANS_QUEUE_SZ; i++) {
		tx_queue_desc[i].buffer_addr = (uint64_t)PADDR(&tx_buff_queue[i][0]);
		tx_queue_desc[i].cmd = (1 <<3) | (1<<0);
		tx_queue_desc[i].status |= E1000_TXD_STAT_DD;
	}

	BAR0_AT(E1000_TDT) = 0;
	BAR0_AT(E1000_TDH) = 0;
	BAR0_AT(E1000_TCTL) |= E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_COLD & (1 << 18));
	BAR0_AT(E1000_TIPG) = 10;
    
    // ex10
    struct PageInfo *rx_queue_page = page_alloc(ALLOC_ZERO);
    BAR0_AT(E1000_RDBAH) = 0;
    BAR0_AT(E1000_RDBAL) = page2pa(rx_queue_page);
    BAR0_AT(E1000_RDLEN) = REC_QUEUE_SZ * sizeof(struct e1000_rx_desc);
    rx_queue_desc = page2kva(rx_queue_page);
	uint16_t w0, w1, w2;
	read_mac_addr(&w0, &w1, &w2);
	cprintf("w0=%02x, w1=%02x, w3=%02x\n", w0, w1, w2);
    BAR0_AT(E1000_RA) = w0 | (w1 << 16);
    BAR0_AT(E1000_RA + 4) = w2;
    BAR0_AT(E1000_RA + 4) |= E1000_RAH_AV;

    BAR0_AT(E1000_MTA) = 0;

    for(i = 0; i < REC_QUEUE_SZ; i++) {
        rx_queue_desc[i].buffer_addr = (uint64_t)PADDR(&rx_buff_queue[i][0]);
        rx_queue_desc[i].status &= ~E1000_RXD_STAT_DD;
    }
    BAR0_AT(E1000_RDH) = 0;
    BAR0_AT(E1000_RDT) = REC_QUEUE_SZ - 1;

    BAR0_AT(E1000_RCTL) |= E1000_RCTL_EN | E1000_RCTL_SECRC;
	BAR0_AT(E1000_IMS) |= E1000_IMS_TXDW | E1000_IMS_RXT0;
	BAR0_AT(E1000_IMC) &= ~(E1000_IMC_TXDW | E1000_IMC_RXT0);
	irq_setmask_8259A(irq_mask_8259A & ~(1<<11));

	return 0;
}

int transmit(char *buff, int size) {
	int tail = BAR0_AT(E1000_TDT);
	if ((size > BUFF_SIZE) || !(tx_queue_desc[tail].status & E1000_TXD_STAT_DD)) {
		return -E_NIC_BUSY;
	}
	memcpy(tx_buff_queue[tail], buff, size);
	tx_queue_desc[tail].status &= ~E1000_TXD_STAT_DD;
	tx_queue_desc[tail].length = size;
	BAR0_AT(E1000_TDT) = (tail + 1) % TRANS_QUEUE_SZ;
	return 0;
}

int receive(char *buff, int size) {
    int next = (BAR0_AT(E1000_RDT) + 1) % REC_QUEUE_SZ;
    if ((rx_queue_desc[next].status & E1000_RXD_STAT_DD) == 0) {
        return -E_REC_QUEUE_EMPTY;
    }
    rx_queue_desc[next].status &= ~E1000_RXD_STAT_DD;
    if (rx_queue_desc[next].length < size) size = rx_queue_desc[next].length;
    memcpy(buff, rx_buff_queue[next], size);
	BAR0_AT(E1000_RDT) = next;
    return size;
}

void e1000_interrupt(){
	// cprintf("e1000 interrupt\n");
	if (BAR0_AT(E1000_ICR) & E1000_ICR_TXDW){ //TRANSMIT
	// cprintf("Transmit interrupt\n");
		BAR0_AT(E1000_ICR) &= ~E1000_ICR_TXDW;
		int i;
		for (i=0;i<NENV;i++){
			if(envs[i].env_status == ENV_WAITING_FOR_TRANSMIT) {
				envs[i].env_status = ENV_RUNNABLE;
			}
		}
		return;
	}

	if (BAR0_AT(E1000_ICR) & E1000_ICR_RXT0){ //RECEIVE
	// cprintf("Receive interrupt\n");
		BAR0_AT(E1000_ICR) &= ~E1000_ICR_RXT0;
		int i;
		for (i=0;i<NENV;i++){
			if(envs[i].env_status == ENV_WAITING_FOR_REC) {
				envs[i].env_status = ENV_RUNNABLE;
			}
		}
		return;
	}
	
}

uint16_t read_eeprom_from(int addr) {
	BAR0_AT(E1000_EERD) |= (addr << E1000_EEPROM_RW_ADDR_SHIFT) | E1000_EEPROM_RW_REG_START;
	while (!(BAR0_AT(E1000_EERD) & E1000_EEPROM_RW_REG_DONE));
	uint16_t res = BAR0_AT(E1000_EERD) >> E1000_EEPROM_RW_REG_DATA;
	BAR0_AT(E1000_EERD) &= 0;
	return res;
}

void read_mac_addr(uint16_t *w0, uint16_t *w1, uint16_t *w2)
 {
	 *w0 = read_eeprom_from(0);
	 *w1 = read_eeprom_from(0x1);
	 *w2 = read_eeprom_from(0x2);
 }	
