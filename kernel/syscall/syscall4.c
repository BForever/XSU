#include <arch.h>
#include <xsu/syscall.h>

void syscall4(unsigned int status, unsigned int cause, context* context)
{
    *GPIO_LED = context->a0;
}
