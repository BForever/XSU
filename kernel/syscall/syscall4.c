#include <arch.h>
#include <xsu/syscall.h>

void syscall4(unsigned int a0, unsigned int a1, unsigned int a2, unsigned a3)
{
    *GPIO_LED = a0;
}
