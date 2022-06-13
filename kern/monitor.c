// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/e1000.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
#define ONE(num) (num ? 1 : 0)

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{"showmappings", "Display the physical page mappings", show_mappings},
	{"backtrace", "", mon_backtrace},
	{"set-perm","", set_perm},
	{"clear-perm", "", clear_perm},
	{"change-perm", "", change_perm},
	{"dump","",dump},
	{"continue", "", mon_continue},
	{"step", "", mon_step}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int mon_continue(int argc, char** argv, struct Trapframe *tf) {
	uint32_t *eflags = &(tf->tf_eflags);
	*eflags &= ~(FL_TF); // Turn off trap flag
	return -1; // resume env
}

int mon_step(int argc, char** argv, struct Trapframe *tf) {
	uint32_t *eflags = &(tf->tf_eflags);
	*eflags |= FL_TF; // Turn on trap flag for single-stepping
	return -1;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

void helper_function(int* base_pointer);

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");
	int* bp=(int*)read_ebp();
	helper_function(bp);
	return 0;
}

void helper_function(int* base_pointer)
{
	int* bp=base_pointer;
	cprintf("ebp %08x eip %08x args", bp,*(bp+1));
	bp+=2;
    int i;
	for (i=0;i<5;i++){
		cprintf(" %08x",*bp);
		bp+=1;
	}
	cprintf("\n");
    struct Eipdebuginfo info;
    debuginfo_eip(*(base_pointer+1), &info); 
    cprintf("\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, (*(base_pointer+1) - info.eip_fn_addr));
	if (*base_pointer == 0 || *base_pointer >= 0xf0110000)
		return;
    helper_function((int*)(*base_pointer));
}

int hextable(char c) {
	if (c >= '0' && c <= '9') return (int)c - '0';
	if (c >= 'a') c -= ('a' - 'A');
	switch (c) {
		case 'A': return 10;
		case 'B': return 11;
		case 'C': return 12;
		case 'D': return 13;
		case 'E': return 14;
		case 'F': return 15;
	}
	return -1;
}

		
int str2hex(char *hexstr) {
	if (hexstr[0] == '0' && hexstr[1] == 'x') hexstr += 2;
	int curr_pow = strlen(hexstr) - 1;
	int res = 0;
	while(*hexstr) {
		int curr = hextable(*hexstr);
		if (curr == -1) return -1;
        res += curr << 4 * curr_pow;
		hexstr++;
		curr_pow--;
	}
	return res;
}

// Prints the pa mapped to every page from the first address until the second
int show_mappings(int argc, char **argv, struct Trapframe *tf) {
	if (argc != 3) {
		cprintf("Error: Invalid number of arguments\n");
		return -1;
	}
    uintptr_t start_addr = str2hex(argv[1]);
	uintptr_t end_addr = str2hex(argv[2]);
	if (start_addr < 0 || end_addr < 0 || start_addr > end_addr || end_addr > ~0) {
		cprintf("Error: Start or end addresses are invalid\n");
		return -1;
	}
    start_addr = PTE_ADDR(start_addr);
	cprintf("Virtual Address    Physical Address    P W U T C A D S G\n"); 
	for(; start_addr <= end_addr; start_addr += PGSIZE) {
		pte_t * pte;
		struct PageInfo * pp = page_lookup(kern_pgdir, (void *)start_addr, &pte);
		cprintf("%08x           %08x            %d %d %d %d %d %d %d %d %d\n",
		        start_addr, page2pa(pp), ONE(*pte & PTE_P),
										 ONE(*pte & PTE_W),
									     ONE(*pte & PTE_U),
									     ONE(*pte & PTE_PWT),
									     ONE(*pte & PTE_PCD),
									     ONE(*pte & PTE_A),
									     ONE(*pte & PTE_D),
									     ONE(*pte & PTE_PS),
									     ONE(*pte & PTE_G));
	}
	return 0;
}

//gets va and int which has permissions
int set_perm (int argc, char **argv, struct Trapframe *tf){ //int va, int perm
	if (argc != 3) cprintf("Incorrect amount of arguments\n");
	uintptr_t va_hex = str2hex(argv[1]);
	uint32_t perm = 0xFFF & str2hex(argv[2]);
	if (va_hex == -1 || perm == -1) {
		cprintf("Error: Invalid arguments\n");
		return -1;
	}
	void* va  = (void *)va_hex;
	pte_t* page_table = pgdir_walk(kern_pgdir, va, 0);
	if(!page_table) {
		cprintf("No page at this address\n");
		return -1;
	}
	*page_table |= perm;
	return 0;
}

//gets va and clears all permissions for that address
int clear_perm(int argc, char **argv, struct Trapframe *tf){ //int va
	if (argc != 3) cprintf("Incorrect amount of arguments\n");

	uintptr_t va_hex = str2hex(argv[1]);
	uint32_t perm = 0xFFF & str2hex(argv[2]);
	if (va_hex == -1 || perm == -1) {
		cprintf("Error: Invalid arguments\n");
		return -1;
	}
	void* va  = (void *)va_hex;
	pte_t* page_table = pgdir_walk(kern_pgdir, va, 0);
	if(!page_table) {
		cprintf("No page at this address\n");
		return -1;
	}
	*page_table &= ~perm;
	return 0;
}

//gets va and permissions that the user wants to change
int change_perm(int argc, char **argv, struct Trapframe *tf){ //int va, int perm
	if (argc != 3) cprintf("Incorrect amount of arguments\n");
	uintptr_t va_hex = str2hex(argv[1]);
	uint32_t perm = 0xFFF & str2hex(argv[2]);
	if (va_hex == -1 || perm == -1) {
		cprintf("Error: Invalid arguments\n");
		return -1;
	}
	void* va  = (void *)va_hex;
	pte_t* page_table = pgdir_walk(kern_pgdir, va, 0);
	if(!page_table) {
		cprintf("No page at this address\n");
		return -1;
	}
	*page_table = *page_table^perm;
	return 0;
}

int dump (int argc, char** argv, struct Trapframe* tf){
	if(argc != 4){
		cprintf("Incorrect number of arguments\n");
		return -1;		
	}
	if(strcmp(argv[1],"virtual") == 0) {
		uint32_t v_addr = str2hex(argv[2]), end_addr = str2hex(argv[3]);
		void* va;
		pte_t* page_table;
		for (; v_addr <= end_addr; v_addr+=PGSIZE){
			v_addr = PTE_ADDR(v_addr);
			va = (void*) v_addr;
			page_table = pgdir_walk(kern_pgdir, va, 0);
			if (!page_table	)
				cprintf("No physical page mapped to address %08x\n",v_addr);
			else //print page
				cprintf("Virtual address: %08x    Physical address: %08x    Memory: %32x",v_addr,*page_table,*(uint32_t*)v_addr);	
		}	
		return 0;
	}
	else if (strcmp(argv[1],"physical")==0){
		uint32_t pa =  str2hex(argv[2]);
		uint32_t pa_end =  str2hex(argv[3]);
		uint32_t va;
		//uint32_t va_end = (uint32_t) KADDR(pa_end);
		for (; pa <= pa_end; pa+= PGSIZE){
			va = (uint32_t) KADDR(pa);	
			cprintf("Virtual address: %08x    Physical address: %08x    Memory: %32x",va,pa,*(uint32_t*)va);
		}
		return 0;
	}
	else{
		cprintf("Must choose \"physical\" or \"virtual\" as address type (second argument)\n");
		return -1;
	}
	
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	transmit("Hello", 6);
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
