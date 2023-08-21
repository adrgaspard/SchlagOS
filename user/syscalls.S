#include "../shared/consts.h"

#define SYSCALL0ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	int $CODE_SYSCALL;      \
	ret

#define SYSCALL1ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	pushl %ebx;           \
	movl 8(%esp), %ebx;   \
	int $CODE_SYSCALL;      \
	popl %ebx;            \
	ret

#define SYSCALL2ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	pushl %ebx;           \
	movl 8(%esp), %ebx;   \
	movl 12(%esp), %ecx;  \
	int $CODE_SYSCALL;      \
	popl %ebx;            \
	ret

#define SYSCALL3ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	pushl %ebx;           \
	movl 8(%esp), %ebx;   \
	movl 12(%esp), %ecx;  \
	movl 16(%esp), %edx;  \
	int $CODE_SYSCALL;      \
	popl %ebx;            \
	ret

#define SYSCALL4ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	pushl %ebx;           \
	movl 8(%esp), %ebx;   \
	movl 12(%esp), %ecx;  \
	movl 16(%esp), %edx;  \
	pushl %esi;           \
	movl 24(%esp), %esi;  \
	int $CODE_SYSCALL;      \
	popl %esi;            \
	popl %ebx;            \
	ret

#define SYSCALL5ARG(FN) \
	.text;                \
	.globl FN;            \
FN:;                      \
	movl $CODE_##FN, %eax;  \
	pushl %ebx;           \
	movl 8(%esp), %ebx;   \
	movl 12(%esp), %ecx;  \
	movl 16(%esp), %edx;  \
	pushl %esi;           \
	movl 24(%esp), %esi;  \
	pushl %edi;           \
	movl 32(%esp), %edi;  \
	int $CODE_SYSCALL;      \
	popl %edi;            \
	popl %esi;            \
	popl %ebx;            \
	ret


SYSCALL2ARG(cons_write)
SYSCALL1ARG(cons_echo)
SYSCALL2ARG(cons_read)

SYSCALL5ARG(start)
SYSCALL1ARG(kill)
SYSCALL1ARG(exit)
SYSCALL2ARG(chprio)
SYSCALL0ARG(getpid)
SYSCALL1ARG(getprio)
SYSCALL2ARG(waitpid)
SYSCALL1ARG(wait_clock)

SYSCALL0ARG(current_clock)
SYSCALL2ARG(clock_settings)

SYSCALL2ARG(pcount)
SYSCALL1ARG(pcreate)
SYSCALL1ARG(pdelete)
SYSCALL2ARG(preceive)
SYSCALL1ARG(preset)
SYSCALL2ARG(psend)