#ifndef _SYSCALL4_H
#define _SYSCALL4_H

#include <syscall.h>
void syscall4(unsigned int status, unsigned int cause, context *pt_context);

#endif
