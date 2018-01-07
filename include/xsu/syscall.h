#ifndef _XSU_SYSCALL_H
#define _XSU_SYSCALL_H

#include <arch.h>
typedef void(*sys_fn)(unsigned int, unsigned int, context*);

extern sys_fn syscalls[256];

void init_syscall();
void syscall(unsigned int status, unsigned int cause, context* pt_context);
void register_syscall(int index, sys_fn fn);

#endif
