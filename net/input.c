#include "ns.h"
#include <inc/error.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
    
    // sys_page_unmap(0, &nsipcbuf);
    sys_page_unmap(0, &nsipcbuf); // just in case
    for(;;)
    {
        sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W | PTE_P);
        int len;
        while ((len = sys_receive(nsipcbuf.pkt.jp_data, PGSIZE - sizeof(int))) == -E_REC_QUEUE_EMPTY)
            sys_yield();
        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U);
        sys_page_unmap(0, &nsipcbuf);
    }
}

