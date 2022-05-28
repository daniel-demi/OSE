// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    pte_t pte = uvpt[(uintptr_t)addr >> PGSHIFT];
    if(!(err & FEC_WR && (pte & PTE_COW))) {
        panic("pgfault: faulting access is write or COW page");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    envid_t id = 0;//sys_getenvid();
    int perms = PTE_P | PTE_U | PTE_W;
    r = sys_page_alloc(id, (void *)PFTEMP, perms);
    if (r < 0) panic("pgfault: couldn't allocate page");
    void * page_addr = (void *)ROUNDDOWN((uintptr_t)addr,PGSIZE);
    memcpy(PFTEMP, page_addr, PGSIZE);
    r = sys_page_map(id, PFTEMP, id, page_addr, perms);
    if (r < 0) panic("pgfault; couldn't remap page");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	// LAB 4: Your code here.
	envid_t curid = sys_getenvid();
    pte_t pte = uvpt[pn];
    if (((pte & PTE_W) || (pte & PTE_COW)) && !(pte & PTE_SHARE)) {
        pte &= ~PTE_W;
        pte |= PTE_COW;
    } else {
		return sys_page_map(curid, (void *)(pn * PGSIZE), envid, 
		(void*)(pn * PGSIZE), pte & PTE_SYSCALL);
	}
    r = sys_page_map(curid, (void *)(pn * PGSIZE), envid, (void*)(pn * PGSIZE), 
    pte & PTE_SYSCALL);
    if(r < 0) return r;
    r = sys_page_map(curid, (void *)(pn * PGSIZE), curid, (void*)(pn * PGSIZE), 
    pte & PTE_SYSCALL);
	return r;
}


//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//set page handler
	set_pgfault_handler(pgfault);
	//create child 
	int id = sys_exofork();
	if (id<0) return id;
	if (id==0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return id;
	}

    
    unsigned xstack = (UXSTACKTOP - PGSIZE);
    int i;
    for(i = 0; i < NPDENTRIES; i++) {
        if(uvpd[i] & PTE_P) {
            int j;
            for(j = 0; j < NPTENTRIES; j++) {
                unsigned num = i*NPTENTRIES + j;
                unsigned top = UTOP >> PGSHIFT;
                if(num >= top) break;
                if(num != (xstack >> PGSHIFT) && (uvpt[num] & PTE_P)) duppage(id, num);
            }
        }
    }
    extern void _pgfault_upcall();
    sys_env_set_pgfault_upcall(id, _pgfault_upcall);
    sys_page_alloc(id, (void*)xstack, PTE_W | PTE_U);

	//mark child as runnable and return 
	sys_env_set_status(id,ENV_RUNNABLE);
	return id;
}                    

                

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
