#ifndef _XSU_SYSCALL_H
#define _XSU_SYSCALL_H

#include <arch.h>
typedef void(*sys_fn)(unsigned int, unsigned int, context*);

extern sys_fn syscalls[256];

void init_syscall();
void syscall(unsigned int status, unsigned int cause, context* pt_context);
void register_syscall(int index, sys_fn fn);

//syscall code allocation
#define SYSCALL_MALLOC 1
#define SYSCALL_FREE   2
#define SYSCALL_EXIT   3
#define SYSCALL_GPIO   4
#define SYSCALL_KILL   5
#define SYSCALL_PRINTTASKS 6
#define SYSCALL_SCHEDULE 7
#define SYSCALL_FORK 8
#define SYSCALL_SLEEP 9


#endif
