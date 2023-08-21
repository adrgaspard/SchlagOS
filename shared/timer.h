#ifndef __TIMER_H__
#define __TIMER_H__

extern void tic_PIT();

extern void clock_init();

// Primitive functions 

extern void clock_settings(unsigned long *quartz, unsigned long *ticks);

extern unsigned long current_clock();

#endif