#ifndef _XSU_SYSCALL_H
#define _XSU_SYSCALL_H

#include <exc.h>

typedef void(*sys_fn)(unsigned int status, unsigned int cause, context* context);

extern sys_fn syscalls[256];

void init_syscall();
void syscall(unsigned int status, unsigned int cause, context* context);
void register_syscall(int index, sys_fn fn);

#endif
