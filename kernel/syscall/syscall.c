#include "syscall4.h"
#include <exc.h>
#include <xsu/syscall.h>
#include <xsu/pc.h>

sys_fn syscalls[256];

void init_syscall()
{
    register_exception_handler(8, syscall);

    // register all syscalls here.
    register_syscall(4, syscall4);
}

void syscall(unsigned int status, unsigned int cause, context* context)
{
    context->v0 &= 255;
    context->epc += 4;
    if (syscalls[context->v0]) {
        syscalls[context->v0](status,cause,context);
    }
}

void register_syscall(int index, sys_fn fn)
{
    index &= 255;
    syscalls[index] = fn;
}
