#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include<stdbool.h>

extern void console_putbytes(const char *str, int length);

extern void print_top_right(const char *str);

// Primitives

extern void cons_write(const char *str, long size);

extern unsigned long cons_read(char *str, unsigned long length);

extern void cons_echo(bool on);

#endif
