#ifndef __PROCESS_H__
#define __PROCESS_H__

#include<stdbool.h>

void process_table_init(void);

void schedule(void);

void request_zombify(int pid);

void request_destroy(int pid);

void clock_tick(void);

bool awake(int pid);

void block_by_message(int pid);

// Primitive functions

int start(int (*program)(void*), unsigned long stack_size, int priority, const char *name, void *argument);

void exit(int retval);

int getpid(void);

int kill(int pid);

int waitpid(int pid, int *retvalp);

int getprio(int pid);

int chprio(int pid, int newprio);

void wait_clock(unsigned long clock);

#endif