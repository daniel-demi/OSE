#include "ns.h"

#ifndef TESTOUTPUT_COUNT
#define TESTOUTPUT_COUNT 10
#endif

static envid_t output_envid;

static struct jif_pkt *pkt = (struct jif_pkt*)REQVA;

#define SERO_COPY_POC_TX 0


void
umain(int argc, char **argv)
{
	envid_t ns_envid = sys_getenvid();
	int i, r;
	binaryname = "testoutput";

	output_envid = fork();
	if (output_envid < 0)
		panic("error forking");
	else if (output_envid == 0) {
		output(ns_envid);
		return;
	}
	for (i = 0; i < TESTOUTPUT_COUNT; i++) {
		if ((r = sys_page_alloc(0, pkt, PTE_P|PTE_U|PTE_W)) < 0)
			panic("sys_page_alloc: %e", r);
		pkt->jp_len = snprintf(pkt->jp_data,
				       PGSIZE - sizeof(pkt->jp_len),
				       "Packet %02d", i);
		if (SERO_COPY_POC_TX) {
			cprintf("The packet before the transmition is:\n");
			int j;
			for (j = 0; j < pkt->jp_len; j++) cprintf("%02x ", pkt->jp_data[j]);
			cprintf("\n");
			cprintf("Transmitting packet %d\n", i);
			ipc_send(output_envid, NSREQ_OUTPUT, pkt, PTE_P|PTE_W|PTE_U);
			for(;;) {
				envid_t from;
				ipc_recv(&from, 0, 0); // wait for a signal from ouput env that it finished transmitting
				if (from == output_envid) break;
			}
			sys_change_tx_pkt();
			cprintf("The packet after change transmtion:\n");
			for (j = 0; j < pkt->jp_len; j++) cprintf("%02x ", pkt->jp_data[j]);
			cprintf("\n\n");
		} else {
			cprintf("Transmitting packet %d\n", i);
			ipc_send(output_envid, NSREQ_OUTPUT, pkt, PTE_P|PTE_W|PTE_U);
		}
		sys_page_unmap(0, pkt);
	}

	// Spin for a while, just in case IPC's or packets need to be flushed
	for (i = 0; i < TESTOUTPUT_COUNT*2; i++) {
		sys_yield();
	}
}
