#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
	//~ if(curenv) {
		//~ cprintf("dbg: %s:%d\n",__FILE__,__LINE__);
		//~ int curr_idx = ENVX(curenv->env_id);
		//~ int i;
		//~ for(i = 0; i < NENV; i++) {
			//~ int idx = (curr_idx + i + 1) % NENV;
			//~ if(envs[i].env_status == ENV_RUNNABLE) {
				//~ env_run(&envs[idx]);
				//~ break;
			//~ }
		//~ }
		//~ if (curenv->env_status == ENV_RUNNING) {
			//~ env_run(curenv);
		//~ }
	//~ }
	//~ // sched_halt never returns
	//~ sched_halt();
	//~ panic("shced_yield: returned");
	
	int begin = curenv ? ENVX(curenv->env_id) + 1 : 0;
	int index = begin;
	bool found = false;
	int i;
	for (i = 0; i < NENV; i++) {
	index = (begin + i) % NENV;
	if (envs[index].env_status == ENV_RUNNABLE) {
	found = true;
	break;
	}
	}
	
	if (found) {
	env_run(&envs[index]);
	} else if (curenv && curenv->env_status == ENV_RUNNING) {
	env_run(curenv);
	} else {
	sched_halt();
	}
	panic("sched_yield attempted to return");
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	//~ cprintf("dbg: sched_halt start\n");
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	//~ cprintf("%s:%d\n",__FILE__,__LINE__);
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}
	//~ cprintf("%s:%d\n",__FILE__,__LINE__);

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));
	//~ cprintf("%s:/%d\n",__FILE__,__LINE__);

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);
	//~ cprintf("%s:%d\n",__FILE__,__LINE__);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();
	//~ cprintf("%s:%d\n",__FILE__,__LINE__);

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
	//~ cprintf("dbg: sched_halt end\n");
}

