#include <kern/e1000.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <kern/e1000_hw.h>

#define BAR0_AT(off) (bar0[off / 4])
#define NDESC 64
#define BUFF_SIZE 2048

// LAB 6: Your driver code here
static volatile uint32_t *bar0;
struct e1000_tx_desc *tx_queue_desc;
char tx_buff_queue[NDESC][BUFF_SIZE];

int attach_e1000(struct pci_func *pcif) {
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
	for (i = 0; i < 64; i++) {
		tx_queue_desc[i].buffer_addr = (uint64_t)PADDR(&tx_buff_queue[i][0]);
		tx_queue_desc[i].cmd = (1 <<3) | (1<<0);
		tx_queue_desc[i].status |= E1000_TXD_STAT_DD;
	}
	BAR0_AT(E1000_TDT) = BAR0_AT(E1000_TDH) = 0;
	BAR0_AT(E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_COLD & (0x40 << 12));
	BAR0_AT(E1000_TIPG) = 10;
	//~ 
	
	
	
	
	return 0;
}
