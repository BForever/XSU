#ifndef _SYSCALL4_H
#define _SYSCALL4_H

#include <xsu/syscall.h>
void syscall4(unsigned int status, unsigned int cause, context *pt_context);

#endif
