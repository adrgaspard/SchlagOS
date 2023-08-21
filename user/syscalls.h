#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

extern int chprio(int pid, int newprio);
extern void cons_write(const char *str, unsigned long size);
extern int cons_read(char *str, unsigned long size);
extern void cons_echo(int on);
extern void exit(int retval);
extern int getpid(void);
extern int getprio(int pid);
extern int kill(int pid);
extern int pcount(int fid, int *count);
extern int pcreate(int count);
extern int pdelete(int fid);
extern int preceive(int fid,int *message);
extern int preset(int fid);
extern int psend(int fid, int message);
extern void clock_settings(unsigned long *quartz, unsigned long *ticks);
extern unsigned long current_clock(void);
extern void wait_clock(unsigned long wakeup);
extern int start(int (*ptfunc)(void *), unsigned long ssize, int prio, const char *name, void *arg);
extern int waitpid(int pid, int *retval);

#endif