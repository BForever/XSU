#include "syscall4.h"

void syscall4(unsigned int status, unsigned int cause, context* context)
{
    *GPIO_LED = context->a0;
}
