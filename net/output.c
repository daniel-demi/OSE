#include "ns.h"
#include <lib/ipc.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	for(;;) {
		envid_t from;
		int perm;
		int res = ipc_recv(&from, &nsipcbuf, &perm);
		if (res) {
			panic("net/output.c: error: %e", r);
		}
		if (res != NSREQ_OUTPUT || ns_envid != from || !(perm & PTE_P))
		{
			cprintf("output receive invalid: res: %d, from: %08x, ns_envid: %08x, PTE_P: %d\n", res, from, ns_envid, perm & PTE_P ? 1 : 0);
			sys_page_unmap(0, &nsipcbuf);
			continue;
		}
		int i;
		for(i = 0; i < nsipcbuf.pkt.jp_len; i += BUFF_SIZE) {
			int size = BUFF_SIZE;
			if (size > nsipcbuf.pkt.jp_len - i) 
				size = nsipcbuf.pkt.jp_len - i;
			for(;;) {
				int res = sys_transmit(nsipcbuf.pkt.data + i, size);
				if (!res) break;
				sys_yield();
			}
		}
	}
}
