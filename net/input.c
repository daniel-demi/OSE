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
    sys_page_unmap(0, &nsipcbuf); // just in case
    for(;;)
    {
        envid_t eid = sys_getenvid();
        int data_size = PGSIZE - sizeof(int);
        int res = sys_update_rx_info(eid, &nsipcbuf, data_size);
        if (res < 0) {
            panic("Couldn't update data: %e", res);
        }
        int len;
        while ((len = sys_receive(eid, data_size)) == -E_REC_QUEUE_EMPTY) {
            if (len == -E_BAD_ENV || len == -E_NO_MEM) {
                panic("Couldn't sys_receive: %e", len);
            }
            sys_env_set_status(eid, ENV_WAITING_FOR_REC);
            sys_yield();
        }
        nsipcbuf.pkt.jp_len = len;
        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U);
        sys_page_unmap(0, &nsipcbuf);

    }
}

