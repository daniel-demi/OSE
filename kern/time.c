#include <kern/time.h>
#include <inc/assert.h>

static double ticks;
static double speed;

void
time_init(void)
{
	ticks = 0.0;
	speed = 1.0;
}

// This should be called once per timer interrupt.  A timer interrupt
// fires every 10 ms.
void
time_tick(void)
{
	ticks += speed;
	if (ticks * 10 < ticks)
		panic("time_tick: time overflowed");
}

unsigned int
time_msec(void)
{
	return (int)(ticks * 10);
}

void change_speed (double new_speed){
	speed = new_speed;
}
